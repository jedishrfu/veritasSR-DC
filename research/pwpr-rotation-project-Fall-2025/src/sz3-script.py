import numpy as np
from pysz import sz, pyConfig
data = np.random.rand(100,100,100).astype(np.float32)
config = pyConfig(data.shape)
config.errorBoundMode = config.EB.ABS
config.absErrorBound = 1e-3
compressed, ratio = sz.compress(data, config)
decompressed = sz.decompress(compressed, np.float32, data.shape, config)
