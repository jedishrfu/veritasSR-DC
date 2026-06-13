/**
 * Equation Discovery Visualizer (Processing / Java mode)
 *
 * Requested changes:
 *  - Restore full button menu (load, expr nav, const nav, const +/- , CSV X/Y selectors)
 *  - Only colorize the DATA series:
 *      * Expected/actual = blue
 *      * Computed/predicted = red
 *    (Plot frames/grid remain neutral.)
 *
 * Also includes:
 *  - Load File button (CSV or binary f32)
 *  - Prompt: "How many data points should be read?" for BOTH CSV and binary
 *  - CSV: choose X and Y columns via X< X> and Y< Y> buttons
 *  - Binary f32: x=index, y=value
 */

import java.io.*;
import java.nio.*;
import java.util.*;
import javax.swing.JOptionPane;

// ----------------------------- Config -----------------------------
final int NUM_EXPRESSIONS = 25;
final int MAX_DEPTH = 4;
final int MAX_CONSTS_PER_EXPR = 6;

final int CONST_MIN = -10;
final int CONST_MAX =  10;

// Numerics safety
final float EPS_DIV  = 1e-6f;
final float LOG_EPS  = 1e-6f;
final float EXP_CLIP = 20.0f;   // clip input to exp
final float TAN_CLIP = 20.0f;   // clip tan output
final float Y_CLIP   = 1e6f;    // clip final y

// UI layout
final int PAD   = 16;
final int TOP_H = 150;
final int BOT_H = 110;

// Series colors (ONLY for data)
final int ACTUAL_COLOR = color(0, 0, 255);   // blue
final int PRED_COLOR   = color(255, 0, 0);   // red

// ----------------------------- Data -----------------------------
float[] xData = new float[] {0, 1};
float[] yData = new float[] {0, 1};
String dataSourceLabel = "Demo data";

// ----------------------------- CSV state -----------------------------
boolean csvMode = false;
String[] csvHeader = null;
float[][] csvCols = null; // [col][row]
int csvXCol = 0;
int csvYCol = 0;

// ----------------------------- Expressions -----------------------------
ArrayList<Expr> exprs = new ArrayList<Expr>();
int exprIndex = 0;
int activeConstSlot = 0;

// ----------------------------- UI -----------------------------
ArrayList<Button> buttons = new ArrayList<Button>();

void setup() {
  size(1200, 760);
  surface.setTitle("Equation Discovery Visualizer");

  textFont(createFont("Menlo", 14));

  buildExpressions();
  buildButtons();

  makeDemoData(300);
}

void draw() {
  background(248);

  Expr e = exprs.get(exprIndex);

  // clamp active constant slot
  if (e.constCount() == 0) activeConstSlot = 0;
  else activeConstSlot = constrain(activeConstSlot, 0, e.constCount()-1);

  float[] yHat = evalExprOnData(e);
  float mse = mse(yData, yHat);

  drawHeader(e, mse);
  drawPlot(yHat);
  drawBottomBar();
  drawButtons();
}

void mousePressed() {
  for (Button b : buttons) {
    if (b.hit(mouseX, mouseY)) {
      b.onClick();
      break;
    }
  }
}

// ============================================================
// File loading
// ============================================================

void loadFile() {
  selectInput("Select CSV or binary f32 file", "fileSelected");
}

void fileSelected(File f) {
  if (f == null) return;

  String name = f.getName().toLowerCase();
  if (name.endsWith(".csv")) loadCSV(f);
  else loadBinaryF32(f);
}

int askDataCount(int maxAvailable) {
  String s = JOptionPane.showInputDialog(
    "How many data points should be read?\n" +
    "(1 .. " + maxAvailable + ", blank = all)");

  if (s == null) return maxAvailable; // cancel => all
  s = s.trim();
  if (s.length() == 0) return maxAvailable;

  int n = maxAvailable;
  try {
    n = Integer.parseInt(s);
  } catch(Exception ex) {
    try { n = (int)Float.parseFloat(s); }
    catch(Exception ex2) { n = maxAvailable; }
  }
  if (n <= 0) n = maxAvailable;
  return constrain(n, 1, maxAvailable);
}

void loadBinaryF32(File f) {
  try {
    byte[] bytes = loadBytes(f.getAbsolutePath());
    if (bytes == null || bytes.length < 4) {
      println("Binary file too small.");
      return;
    }

    FloatBuffer fb = ByteBuffer.wrap(bytes)
      .order(ByteOrder.LITTLE_ENDIAN)
      .asFloatBuffer();

    int total = fb.capacity();
    int n = askDataCount(total);

    xData = new float[n];
    yData = new float[n];
    for (int i = 0; i < n; i++) {
      xData[i] = i;
      yData[i] = fb.get(i);
    }
    normalizeXToMinus1Plus1();

    csvMode = false;
    csvHeader = null;
    csvCols = null;

    dataSourceLabel = "Binary f32 (" + f.getName() + "), N=" + n;
  } catch(Exception ex) {
    println("Binary load error: " + ex);
  }
}

void loadCSV(File f) {
  try {
    String[] lines = loadStrings(f.getAbsolutePath());
    if (lines == null || lines.length < 1) return;

    // find first non-empty line
    int first = -1;
    for (int i = 0; i < lines.length; i++) {
      if (trim(lines[i]).length() > 0) { first = i; break; }
    }
    if (first < 0) return;

    String[] firstParts = splitCSVLine(lines[first]);
    boolean hasHeader = !allNumeric(firstParts);
    int dataStart = hasHeader ? first + 1 : first;

    // find first data line
    int start = -1;
    for (int i = dataStart; i < lines.length; i++) {
      if (trim(lines[i]).length() > 0) { start = i; break; }
    }
    if (start < 0) return;

    String[] probe = hasHeader ? firstParts : splitCSVLine(lines[start]);
    int colCount = max(1, probe.length);

    csvHeader = new String[colCount];
    if (hasHeader) {
      for (int c = 0; c < colCount; c++) {
        String h = (c < firstParts.length) ? trim(firstParts[c]) : ("col" + c);
        if (h.length() == 0) h = "col" + c;
        csvHeader[c] = h;
      }
    } else {
      for (int c = 0; c < colCount; c++) csvHeader[c] = "col" + c;
    }

    ArrayList<String[]> rows = new ArrayList<String[]>();
    for (int i = start; i < lines.length; i++) {
      String s = trim(lines[i]);
      if (s.length() == 0) continue;
      String[] parts = splitCSVLine(s);
      if (parts.length < colCount) {
        String[] padded = new String[colCount];
        for (int k = 0; k < colCount; k++) padded[k] = (k < parts.length) ? parts[k] : "";
        parts = padded;
      }
      rows.add(parts);
    }
    if (rows.size() < 2) {
      println("CSV did not contain enough rows.");
      return;
    }

    int available = rows.size();
    int want = askDataCount(available);

    // convert rows -> column-major floats, skipping non-numeric
    float[][] tmp = new float[colCount][want];
    int kept = 0;
    for (int r = 0; r < want; r++) {
      String[] parts = rows.get(r);
      float[] vals = new float[colCount];
      boolean ok = true;
      for (int c = 0; c < colCount; c++) {
        float v = parseFloatSafe(parts[c]);
        if (!isFinite(v)) { ok = false; break; }
        vals[c] = v;
      }
      if (!ok) continue;
      for (int c = 0; c < colCount; c++) tmp[c][kept] = vals[c];
      kept++;
    }
    if (kept < 2) {
      println("CSV parse failed (not enough numeric rows).");
      return;
    }

    csvCols = new float[colCount][kept];
    for (int c = 0; c < colCount; c++) arrayCopy(tmp[c], csvCols[c], kept);

    csvMode = true;
    csvXCol = 0;
    csvYCol = min(1, colCount-1);
    rebuildXYFromCSV();

    dataSourceLabel = "CSV (" + f.getName() + "), N=" + kept + (hasHeader ? " (header)" : " (no header)");
  } catch(Exception ex) {
    println("CSV load error: " + ex);
  }
}

String[] splitCSVLine(String line) {
  String[] parts = split(line, ','); // numeric CSV assumption (no quoted commas)
  for (int i = 0; i < parts.length; i++) parts[i] = trim(parts[i]);
  return parts;
}

boolean allNumeric(String[] parts) {
  for (String p : parts) {
    if (trim(p).length() == 0) return false;
    float v = parseFloatSafe(p);
    if (!isFinite(v)) return false;
  }
  return true;
}

float parseFloatSafe(String s) {
  try { return Float.parseFloat(trim(s)); }
  catch(Exception e) { return Float.NaN; }
}

boolean isFinite(float v) {
  return !Float.isNaN(v) && !Float.isInfinite(v);
}

void rebuildXYFromCSV() {
  if (!csvMode || csvCols == null) return;

  int n = csvCols[0].length;
  xData = new float[n];
  yData = new float[n];

  arrayCopy(csvCols[csvXCol], xData);
  arrayCopy(csvCols[csvYCol], yData);

  normalizeXToMinus1Plus1();
}

void normalizeXToMinus1Plus1() {
  float mn = +Float.MAX_VALUE, mx = -Float.MAX_VALUE;
  for (float v : xData) { mn = min(mn, v); mx = max(mx, v); }
  if (mn == mx) return;
  for (int i = 0; i < xData.length; i++) {
    float t = (xData[i] - mn) / (mx - mn);
    xData[i] = lerp(-1, 1, t);
  }
}

// ============================================================
// Demo data
// ============================================================

void makeDemoData(int n) {
  xData = new float[n];
  yData = new float[n];
  for (int i = 0; i < n; i++) {
    float x = map(i, 0, n-1, -3, 3);
    xData[i] = x;
    float y = 2.0f*sin(1.2f*x) + 0.4f*cos(2.7f*x) + 0.15f*x*x;
    y += 0.15f*(random(-1, 1));
    yData[i] = y;
  }
  normalizeXToMinus1Plus1();
  dataSourceLabel = "Demo data, N=" + n;
}

// ============================================================
// Expressions & scoring
// ============================================================

void buildExpressions() {
  exprs.clear();
  randomSeed(7);
  ExprFactory factory = new ExprFactory();
  for (int i = 0; i < NUM_EXPRESSIONS; i++) {
    exprs.add(factory.randomExpr(MAX_DEPTH, MAX_CONSTS_PER_EXPR));
  }
}

float[] evalExprOnData(Expr e) {
  float[] yHat = new float[yData.length];
  for (int i = 0; i < yHat.length; i++) yHat[i] = e.eval(xData[i]);
  return yHat;
}

float mse(float[] a, float[] b) {
  double s = 0.0;
  int cnt = 0;
  int n = min(a.length, b.length);
  for (int i = 0; i < n; i++) {
    float va = a[i], vb = b[i];
    if (!isFinite(va) || !isFinite(vb)) continue;
    double d = (double)vb - (double)va;
    s += d*d;
    cnt++;
  }
  if (cnt == 0) return Float.POSITIVE_INFINITY;
  return (float)(s / cnt);
}

// ============================================================
// Drawing
// ============================================================

void drawHeader(Expr e, float mse) {
  fill(255);
  stroke(220);
  rect(0, 0, width, TOP_H);

  fill(30);
  noStroke();
  textSize(16);
  text("Expression " + (exprIndex+1) + " / " + exprs.size(), PAD, 28);

  textSize(12);
  text("Data: " + dataSourceLabel, PAD, 48);

  textSize(14);
  text("f(x) = " + e.toStringSubstituted(), PAD, 76);

  textSize(12);
  if (e.constCount() == 0) {
    text("Constants: (none)", PAD, 98);
  } else {
    text("Editing constant c" + activeConstSlot + " = " + e.getConst(activeConstSlot) +
         "   (range " + CONST_MIN + ".." + CONST_MAX + ")   total: " + e.constCount(),
         PAD, 98);
  }

  textSize(13);
  text("Least-squares score (MSE): " + nf(mse, 0, 6), PAD, 124);

  if (csvMode && csvHeader != null) {
    textSize(12);
    String xName = csvHeader[csvXCol];
    String yName = csvHeader[csvYCol];
    text("CSV columns: X=" + xName + "   Y=" + yName + "   (use X< X> and Y< Y> buttons)", PAD, 144);
  }
}

void drawPlot(float[] yHat) {
  int plotX = PAD;
  int plotY = TOP_H + PAD;
  int plotW = width - 2*PAD;
  int plotH = height - TOP_H - BOT_H - 2*PAD;

  // neutral plot frame
  fill(255);
  stroke(220);
  rect(plotX, plotY, plotW, plotH);

  // determine y range
  float yMin = +Float.MAX_VALUE, yMax = -Float.MAX_VALUE;
  for (int i = 0; i < yData.length; i++) {
    float a = yData[i];
    float p = yHat[i];
    if (isFinite(a)) { yMin = min(yMin, a); yMax = max(yMax, a); }
    if (isFinite(p)) { yMin = min(yMin, p); yMax = max(yMax, p); }
  }
  if (!isFinite(yMin) || !isFinite(yMax) || yMin == yMax) { yMin = -1; yMax = 1; }

  float pad = 0.08f * (yMax - yMin);
  yMin -= pad;
  yMax += pad;

  // neutral grid
  stroke(235);
  for (int k = 1; k <= 4; k++) {
    float yy = lerp(plotY + plotH, plotY, k/5.0f);
    line(plotX, yy, plotX + plotW, yy);
  }
  for (int k = 1; k <= 9; k++) {
    float xx = lerp(plotX, plotX + plotW, k/10.0f);
    line(xx, plotY, xx, plotY + plotH);
  }

  // neutral y labels
  fill(90);
  noStroke();
  textSize(12);
  text(nf(yMax, 0, 3), plotX + 6, plotY + 14);
  text(nf(yMin, 0, 3), plotX + 6, plotY + plotH - 6);

  // ===== DATA SERIES COLORS ONLY =====
  strokeWeight(2);

  // actual / expected (blue)
  stroke(ACTUAL_COLOR);
  drawSeries(plotX, plotY, plotW, plotH, yData, yMin, yMax);

  // predicted / computed (red)
  stroke(PRED_COLOR);
  drawSeries(plotX, plotY, plotW, plotH, yHat, yMin, yMax);

  strokeWeight(1);

  // legend (neutral text)
  fill(30);
  noStroke();
  textSize(12);
  text("Expected/Actual (blue) vs Computed/Predicted (red)", plotX + 10, plotY + plotH - 10);
}

void drawSeries(int px, int py, int pw, int ph, float[] ys, float yMin, float yMax) {
  int n = ys.length;
  float prevX = 0, prevY = 0;
  boolean hasPrev = false;

  for (int i = 0; i < n; i++) {
    float t = (n == 1) ? 0 : (float)i / (n - 1);
    float sx = lerp(px, px + pw, t);

    float v = ys[i];
    if (!isFinite(v)) { hasPrev = false; continue; }

    float u = (v - yMin) / (yMax - yMin);
    float sy = lerp(py + ph, py, constrain(u, 0, 1));

    if (hasPrev) line(prevX, prevY, sx, sy);
    prevX = sx;
    prevY = sy;
    hasPrev = true;
  }
}

void drawBottomBar() {
  fill(255);
  stroke(220);
  rect(0, height - BOT_H, width, BOT_H);

  fill(90);
  noStroke();
  textSize(12);
  text("Load CSV or binary f32. CSV: choose X/Y columns with X< X> and Y< Y>. Binary: X=index, Y=value.",
       PAD, height - 14);
}

// ============================================================
// Buttons
// ============================================================

void buildButtons() {
  buttons.clear();

  int y = height - BOT_H + 18;
  int x = PAD;
  int bh = 44;
  int gap = 12;

  buttons.add(new Button(x, y, 120, bh, "Load File", () -> loadFile()));
  x += 120 + gap;

  buttons.add(new Button(x, y, 120, bh, "Prev Expr", () -> {
    exprIndex = (exprIndex - 1 + exprs.size()) % exprs.size();
    activeConstSlot = 0;
  }));
  x += 120 + gap;

  buttons.add(new Button(x, y, 120, bh, "Next Expr", () -> {
    exprIndex = (exprIndex + 1) % exprs.size();
    activeConstSlot = 0;
  }));
  x += 120 + gap;

  buttons.add(new Button(x, y, 130, bh, "Prev Const", () -> {
    Expr e = exprs.get(exprIndex);
    if (e.constCount() > 0) activeConstSlot = (activeConstSlot - 1 + e.constCount()) % e.constCount();
  }));
  x += 130 + gap;

  buttons.add(new Button(x, y, 130, bh, "Next Const", () -> {
    Expr e = exprs.get(exprIndex);
    if (e.constCount() > 0) activeConstSlot = (activeConstSlot + 1) % e.constCount();
  }));
  x += 130 + gap;

  buttons.add(new Button(x, y, 60, bh, "-1", () -> adjustConst(-1)));
  x += 60 + gap;

  buttons.add(new Button(x, y, 60, bh, "+1", () -> adjustConst(+1)));
  x += 60 + gap;

  // CSV column selection buttons
  buttons.add(new Button(x, y, 60, bh, "X<", () -> {
    if (csvMode && csvHeader != null) { csvXCol = max(0, csvXCol-1); rebuildXYFromCSV(); }
  }));
  x += 60 + 6;

  buttons.add(new Button(x, y, 60, bh, "X>", () -> {
    if (csvMode && csvHeader != null) { csvXCol = min(csvHeader.length-1, csvXCol+1); rebuildXYFromCSV(); }
  }));
  x += 60 + gap;

  buttons.add(new Button(x, y, 60, bh, "Y<", () -> {
    if (csvMode && csvHeader != null) { csvYCol = max(0, csvYCol-1); rebuildXYFromCSV(); }
  }));
  x += 60 + 6;

  buttons.add(new Button(x, y, 60, bh, "Y>", () -> {
    if (csvMode && csvHeader != null) { csvYCol = min(csvHeader.length-1, csvYCol+1); rebuildXYFromCSV(); }
  }));
}

void adjustConst(int delta) {
  Expr e = exprs.get(exprIndex);
  if (e.constCount() == 0) return;
  int v = e.getConst(activeConstSlot) + delta;
  e.setConst(activeConstSlot, v);
}

void drawButtons() {
  for (Button b : buttons) b.draw();
}

interface Action { void run(); }

class Button {
  int x, y, w, h;
  String label;
  Action action;

  Button(int x, int y, int w, int h, String label, Action action) {
    this.x=x; this.y=y; this.w=w; this.h=h;
    this.label=label;
    this.action=action;
  }

  boolean hit(int mx, int my) {
    return (mx >= x && mx <= x+w && my >= y && my <= y+h);
  }

  void onClick() { action.run(); }

  void draw() {
    boolean over = hit(mouseX, mouseY);
    stroke(180);
    fill(over ? 235 : 245);
    rect(x, y, w, h, 10);

    fill(40);
    noStroke();
    textAlign(CENTER, CENTER);
    textSize(14);
    text(label, x + w/2, y + h/2);
    textAlign(LEFT, BASELINE);
  }
}

// ============================================================
// Expression AST
// ============================================================

abstract class Expr {
  int[] consts;
  Expr(int[] consts) { this.consts = consts; }

  abstract float evalNode(float x);
  abstract String toStringRaw();

  float eval(float x) {
    float v = evalNode(x);
    if (!isFinite(v)) return Float.NaN;
    return constrain(v, -Y_CLIP, Y_CLIP);
  }

  int constCount() { return (consts == null) ? 0 : consts.length; }
  int getConst(int i) { return consts[i]; }
  void setConst(int i, int v) { consts[i] = constrain(v, CONST_MIN, CONST_MAX); }
  float c(int idx) { return (consts == null) ? 0 : consts[idx]; }

  String toStringSubstituted() {
    String s = toStringRaw();
    if (consts == null) return s;
    for (int i = 0; i < consts.length; i++) s = s.replace("c"+i, str(consts[i]));
    return s;
  }
}

class VarX extends Expr {
  VarX(int[] consts) { super(consts); }
  float evalNode(float x) { return x; }
  String toStringRaw() { return "x"; }
}

class ConstRef extends Expr {
  int idx;
  ConstRef(int[] consts, int idx) { super(consts); this.idx = idx; }
  float evalNode(float x) { return c(idx); }
  String toStringRaw() { return "c" + idx; }
}

class Binary extends Expr {
  char op;
  Expr a, b;
  Binary(int[] consts, char op, Expr a, Expr b) { super(consts); this.op=op; this.a=a; this.b=b; }

  float evalNode(float x) {
    float va = a.evalNode(x);
    float vb = b.evalNode(x);
    if (!isFinite(va) || !isFinite(vb)) return Float.NaN;

    switch(op) {
      case '+': return va + vb;
      case '-': return va - vb;
      case '*': return va * vb;
      case '/':
        float denom = (abs(vb) < EPS_DIV) ? (vb >= 0 ? EPS_DIV : -EPS_DIV) : vb;
        return va / denom;
    }
    return Float.NaN;
  }

  String toStringRaw() { return "(" + a.toStringRaw() + " " + op + " " + b.toStringRaw() + ")"; }
}

class Unary extends Expr {
  String fn;
  Expr t;
  Unary(int[] consts, String fn, Expr t) { super(consts); this.fn=fn; this.t=t; }

  float evalNode(float x) {
    float v = t.evalNode(x);
    if (!isFinite(v)) return Float.NaN;

    if (fn.equals("sin")) return sin(v);
    if (fn.equals("cos")) return cos(v);
    if (fn.equals("tan")) return constrain(tan(v), -TAN_CLIP, TAN_CLIP);

    if (fn.equals("log")) return log(abs(v) + LOG_EPS);
    if (fn.equals("exp")) return exp(constrain(v, -EXP_CLIP, EXP_CLIP));

    return Float.NaN;
  }

  String toStringRaw() { return fn + "(" + t.toStringRaw() + ")"; }
}

// ============================================================
// Random expression factory (non-static so random() works)
// ============================================================

class ExprFactory {
  String[] UNARY = { "sin", "cos", "tan", "log", "exp" };
  char[]   BINARY = { '+', '-', '*', '/' };

  Expr randomExpr(int maxDepth, int maxConsts) {
    int k = (int)random(0, maxConsts + 1);
    int[] consts = new int[k];
    for (int i = 0; i < k; i++) consts[i] = (int)random(CONST_MIN, CONST_MAX + 1);
    return randomNode(consts, maxDepth);
  }

  Expr randomNode(int[] consts, int depth) {
    if (depth <= 0) return leaf(consts);

    float r = random(1);
    if (r < 0.35f) {
      String fn = UNARY[(int)random(UNARY.length)];
      return new Unary(consts, fn, randomNode(consts, depth - 1));
    } else {
      char op = BINARY[(int)random(BINARY.length)];
      Expr a = randomNode(consts, depth - 1);
      Expr b = randomNode(consts, depth - 1);
      return new Binary(consts, op, a, b);
    }
  }

  Expr leaf(int[] consts) {
    if (consts.length == 0) return new VarX(consts);
    if (random(1) < 0.55f) return new VarX(consts);
    int idx = (int)random(consts.length);
    return new ConstRef(consts, idx);
  }
}
