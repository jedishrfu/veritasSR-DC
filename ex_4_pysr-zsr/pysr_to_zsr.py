#!/usr/bin/env python3
"""Fit independent float32 blocks with PySR and write ex_3 version-2.0 ASTs.
Requires Python >=3.10, numpy, sympy, pysr. See README.md.
"""
import argparse
import json
import math
import os
from pathlib import Path
import struct
import tempfile

# ex_3/include/ast_nodes.h: OpKind values, not Python/SymPy operator numbers.
ADD, SUB, MUL, DIV, POW, SIN, COS, EXP = range(1, 9)


def constant(value):
    value = struct.unpack('<f', struct.pack('<f', float(value)))[0]
    if not math.isfinite(value):
        raise ValueError('Coefficient is not representable as finite float32')
    return ('c', value)


def node_count(t):
    return 1 + sum(node_count(child) for child in t[2:] if isinstance(child, tuple))


def encode(t):
    kind = t[0]
    if kind == 'x': return b'\x00'
    if kind == 'c': return b'\x40' + struct.pack('<f', t[1])
    return bytes([(0x80 if kind == 'u' else 0xc0) | t[1]]) + b''.join(encode(c) for c in t[2:])


def evaluate(t, x):
    if t[0] == 'x': return float(x)
    if t[0] == 'c': return t[1]
    a = evaluate(t[2], x)
    if t[0] == 'u':
        return {SIN: math.sin, COS: math.cos, EXP: math.exp}[t[1]](a)
    b = evaluate(t[3], x)
    if t[1] == ADD: return a+b
    if t[1] == SUB: return a-b
    if t[1] == MUL: return a*b
    if t[1] == DIV: return 0.0 if abs(b) < 1e-12 else a/b
    if t[1] == POW: return math.pow(a,b)
    raise ValueError('Unsupported AST operation')


def expression(t):
    if t[0] == 'x': return 'x'
    if t[0] == 'c': return format(t[1], '.17g')
    if t[0] == 'u':
        return {SIN:'sin', COS:'cos', EXP:'exp'}[t[1]]+'('+expression(t[2])+')'
    return '('+expression(t[2])+{ADD:'+',SUB:'-',MUL:'*',DIV:'/',POW:'^'}[t[1]]+expression(t[3])+')'


def from_sympy(expr, sp, x_tree):
    """Translate structure, never eval a generated string. Constants quantize now."""
    if isinstance(expr, sp.Symbol):
        if str(expr) != 'u': raise ValueError('Unexpected variable: '+str(expr))
        return x_tree
    if expr.is_number:
        return constant(expr)
    if expr.func in (sp.Add, sp.Mul):
        children = [from_sympy(c,sp,x_tree) for c in expr.args]
        t = children[0]
        for c in children[1:]: t = ('b', ADD if expr.func == sp.Add else MUL,t,c)
        return t
    if expr.func == sp.Pow:
        base, exponent = expr.args
        # PySR's multiplication export can introduce integer powers.
        if not exponent.is_Integer or not -16 <= int(exponent) <= 16:
            raise ValueError('Only bounded integer powers can be exported')
        power = int(exponent)
        if power == 0: return constant(1)
        b = from_sympy(base,sp,x_tree); t = b
        for _ in range(abs(power)-1): t = ('b',MUL,t,b)
        return ('b',DIV,constant(1),t) if power < 0 else t
    if expr.func in (sp.sin,sp.cos,sp.exp):
        return ('u',{sp.sin:SIN,sp.cos:COS,sp.exp:EXP}[expr.func],from_sympy(expr.args[0],sp,x_tree))
    raise ValueError('Unsupported expression: '+str(expr))


def zsr_bytes(trees, block_size, tolerance):
    if block_size not in (256,512,1024,2048): raise ValueError('Invalid block size')
    if not 1 <= len(trees) <= 65535: raise ValueError('Format permits 1..65535 blocks')
    tolerance = constant(tolerance)[1]
    if tolerance <= 0: raise ValueError('Tolerance must be positive float32')
    # Version 2.0, flags 0, block size uint32, tolerance float32, count uint16.
    result = bytearray(struct.pack('<BBIfH',0x20,0,block_size,tolerance,len(trees)))
    for t in trees:
        count = node_count(t)
        if count > 65535: raise ValueError('AST exceeds uint16 node count')
        result += struct.pack('<H',count) + encode(t)
    return bytes(result)


def atomic_write(path, content, overwrite=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='.'+path.name+'.',dir=path.parent)
    try:
        with os.fdopen(fd,'wb') as f: f.write(content)
        if overwrite: os.replace(temporary,path)
        else:
            # Atomic no-clobber publication; link fails if destination exists.
            os.link(temporary,path)
            os.unlink(temporary)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input',type=Path,help='Little-endian binary float32 samples')
    p.add_argument('-o','--output',type=Path)
    p.add_argument('--reconstructed-output',type=Path,
                   help='Also write the reconstructed samples as little-endian float32')
    p.add_argument('-b','--block-size',type=int,choices=[256,512,1024,2048],default=1024)
    p.add_argument('-e','--tolerance',type=float,default=.025,help='Absolute error target (default .025)')
    p.add_argument('--iterations',type=int,default=100,help='PySR iterations per nonconstant block')
    p.add_argument('--maxsize',type=int,default=30)
    p.add_argument('--seed',type=int,default=12345)
    p.add_argument('--max-blocks',type=int,help='Process only the first N blocks for a trial')
    p.add_argument('--max-samples',type=int,help='Process only the first N input samples')
    p.add_argument('--require-tolerance',action='store_true',help='Do not publish .zsr if any block misses tolerance')
    p.add_argument('--overwrite',action='store_true')
    p.add_argument('--tail',choices=['pad','error'],default='pad',help='Pad final block by repeating last value; original length is in JSON')
    args = p.parse_args()
    if not math.isfinite(args.tolerance) or args.tolerance <= 0: p.error('tolerance must be finite and positive')
    if args.iterations < 1 or not 3 <= args.maxsize <= 128: p.error('iterations >=1 and maxsize in 3..128 required')
    if not 0 <= args.seed < 2**31: p.error('seed must be in 0..2147483647')
    if args.max_blocks is not None and args.max_blocks < 1: p.error('max-blocks must be positive')
    if args.max_samples is not None and args.max_samples < 1: p.error('max-samples must be positive')
    if args.max_blocks is not None and args.max_samples is not None:
        p.error('use only one of --max-blocks and --max-samples')
    tolerance = constant(args.tolerance)[1]
    if tolerance == 0: p.error('tolerance underflows float32')
    src = args.input.resolve(strict=True)
    dest = (args.output or src.with_suffix('.pysr.zsr')).resolve()
    reconstructed_path = (args.reconstructed_output.resolve()
                          if args.reconstructed_output is not None else None)
    if dest.suffix.lower() != '.zsr': p.error('output must have .zsr extension')
    report_path = dest.with_suffix('.json')
    output_paths = [dest, report_path]
    if reconstructed_path is not None:
        output_paths.append(reconstructed_path)
    if src in output_paths: p.error('output must not replace input')
    if len(set(output_paths)) != len(output_paths): p.error('output paths must be distinct')
    for path in output_paths:
        if path.exists() and not args.overwrite: p.error(str(path)+' exists; use --overwrite')
    if src.stat().st_size == 0 or src.stat().st_size % 4: p.error('input must contain complete float32 values')
    import numpy as np
    import sympy as sp
    data = np.fromfile(src,dtype='<f4').astype(np.float64)
    if not np.isfinite(data).all(): p.error('input contains NaN/Infinity')
    original_count = len(data)
    if args.max_blocks: data = data[:args.max_blocks*args.block_size]
    if args.max_samples: data = data[:args.max_samples]
    count = len(data)
    if count % args.block_size and args.tail == 'error': p.error('partial final block; use --tail pad')
    if math.ceil(count/args.block_size) > 65535: p.error('too many blocks for .zsr uint16 count')
    # Training coordinate [-1,1]; serialized x remains the ex_3 local sample index.
    x_tree = ('b',SUB,('b',MUL,constant(2/(args.block_size-1)),('x',)),constant(1))
    u = np.array([evaluate(x_tree,i) for i in range(args.block_size)])[:,None]
    report = {'format':'ex_3 AST binary 2.0','source':str(src),'original_sample_count':original_count,
              'sample_count':count,'block_size':args.block_size,'tolerance':tolerance,
              'padded_sample_count':math.ceil(count/args.block_size)*args.block_size,
              'coordinate':'block-local x=0..block_size-1','settings':{'iterations':args.iterations,
              'maxsize':args.maxsize,'seed':args.seed,'parallelism':'serial'},'blocks':[]}
    trees=[]
    reconstructed=[]
    for block_id,start in enumerate(range(0,count,args.block_size)):
        real = data[start:start+args.block_size]
        target = np.pad(real,(0,args.block_size-len(real)),mode='edge')
        # Always retain a finite baseline, explicitly identified in the manifest.
        candidates=[(constant(float((target.min()+target.max())/2)),'constant baseline')]
        if not np.all(target == target[0]):
            from pysr import PySRRegressor
            model = PySRRegressor(niterations=args.iterations,maxsize=args.maxsize,
                binary_operators=['+','-','*'],unary_operators=['sin','cos','exp'],
                model_selection='best',precision=64,parallelism='serial',
                deterministic=True,random_state=(args.seed+block_id) % (2**31),
                progress=False,verbosity=0)
            model.fit(u,target,variable_names=['u'])
            for _,row in model.equations_.iterrows():
                try:
                    tree=from_sympy(row['sympy_format'],sp,x_tree)
                    if node_count(tree)<=65535: candidates.append((tree,'PySR'))
                except (ValueError,TypeError,OverflowError,RecursionError): continue
        scored=[]
        for tree,origin in candidates:
            try:
                # Check exactly the float32 constants and output the decoder will use.
                with np.errstate(over='ignore',invalid='ignore'):
                    prediction=np.asarray([evaluate(tree,i) for i in range(args.block_size)],dtype=np.float32).astype(float)
                if not np.isfinite(prediction).all(): continue
                errors=prediction[:len(real)]-real
                scored.append((float(np.max(np.abs(errors))),node_count(tree),float(np.mean(errors**2)),tree,origin))
            except (OverflowError,ValueError,ZeroDivisionError): continue
        if not scored: raise RuntimeError(f'No finite serializable candidate for block {block_id}')
        eligible=[c for c in scored if c[0]<=tolerance]
        best=min(eligible,key=lambda c:(c[1],c[0])) if eligible else min(scored,key=lambda c:(c[0],c[1]))
        error,nodes,mse,tree,origin=best; trees.append(tree)
        reconstructed.extend(evaluate(tree, i) for i in range(len(real)))
        report['blocks'].append({'block':block_id,'start':start,'length':len(real),'nodes':nodes,
            'max_abs_error':error,'rmse':math.sqrt(mse),'within_tolerance':error<=tolerance,
            'origin':origin,'expression':expression(tree)})
        print(f'Block {block_id+1}/{math.ceil(count/args.block_size)}: max error={error:.6g}; nodes={nodes}; '+('PASS' if error<=tolerance else 'TARGET MISSED'),flush=True)
    report['all_within_tolerance']=all(b['within_tolerance'] for b in report['blocks'])
    report['rmse']=math.sqrt(sum(b['rmse']**2*b['length'] for b in report['blocks'])/count)
    report['max_abs_error']=max(b['max_abs_error'] for b in report['blocks'])
    if args.require_tolerance and not report['all_within_tolerance']:
        raise RuntimeError('Tolerance target missed; no outputs published. Increase search budget or relax tolerance.')
    payload=zsr_bytes(trees,args.block_size,tolerance)
    atomic_write(report_path,(json.dumps(report,indent=2,allow_nan=False)+'\n').encode(),args.overwrite)
    atomic_write(dest,payload,args.overwrite)
    if reconstructed_path is not None:
        reconstructed_bytes=np.asarray(reconstructed,dtype='<f4').tobytes()
        atomic_write(reconstructed_path,reconstructed_bytes,args.overwrite)
    print(f'Wrote {dest} ({len(payload)} bytes) and {report_path}')
    if reconstructed_path is not None:
        print(f'Wrote {reconstructed_path} ({len(reconstructed_bytes)} bytes)')
    print(f'Original samples processed: {count}; decoder emits {report["padded_sample_count"]}. Trim to sample_count in JSON.')
    if not report['all_within_tolerance']: print('WARNING: some blocks miss the tolerance target; see JSON. No residual correction is stored.')


if __name__ == '__main__':
    try: main()
    except (ImportError,OSError,ValueError,RuntimeError,OverflowError) as exc:
        raise SystemExit('Error: '+str(exc))
