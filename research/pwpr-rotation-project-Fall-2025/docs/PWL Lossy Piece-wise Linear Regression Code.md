# PWL Lossy Piece-wise Linear Regression Code

This is a Processing sketch to show how a piece-wise linear regression compression could be done.

## Synthetic Data Generation

```java
float[] generateSignal(int n) {
  float[] y = new float[n];
  Random rng = new Random(7);
  for (int i = 0; i < n; i++) {
    // mix: gentle trend + medium/high freq sinusoids + small noise
    double trend = 0.00003 * i;
    double s1 = 0.5 * Math.sin(2 * Math.PI * i / 2000.0);
    double s2 = 0.25 * Math.sin(2 * Math.PI * i / 400.0 + 0.7);
    double noise = 0.01 * rng.nextGaussian();
    y[i] = (float)(trend + s1 + s2 + noise);
  }
  return y;
}
```

## PWL Compress

```java

// Greedy segment grower with least-squares fit per extension.
// For each segment, we extend one point at a time, re-fit y = m*x + b,
// and check max absolute error vs eps. When violated, finalize the previous end.
// This is simple and reasonably fast for ~50k points and small eps.

static class Segment {
  int start, end;   // [start, end) indices
  double m, b;      // y = m*x + b
  Segment(int s, int e, double m_, double b_) { start = s; end = e; m = m_; b = b_; }
}

ArrayList<Segment> pwlCompress(float[] y, float eps) {
  int n = y.length;
  ArrayList<Segment> segs = new ArrayList<Segment>();
  int start = 0;

  while (start < n) {
    int end = start + 2; // need at least 2 pts to define a line
    int bestEnd = start + 1;
    double bestM = 0.0, bestB = y[start];

    while (end <= n) {
      // Fit y = m*x + b on indices [start, end)
      FitResult fr = fitLine(y, start, end);
      double maxErr = maxAbsError(y, start, end, fr.m, fr.b);

      if (maxErr <= eps) {
        bestEnd = end;
        bestM = fr.m;
        bestB = fr.b;
        end++;
      } else {
        break;
      }
    }

    // finalize segment
    segs.add(new Segment(start, bestEnd, bestM, bestB));
    start = bestEnd;
  }
  return segs;
}
```

## FitResult

```java

static class FitResult {
  double m, b;
  FitResult(double m_, double b_) { m = m_; b = b_; }
}

FitResult fitLine(float[] y, int s, int e) {
  // Least squares line fit over integer x = s..e-1
  int n = e - s;
  if (n == 1) return new FitResult(0.0, y[s]);

  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (int i = s; i < e; i++) {
    double xi = i;
    double yi = y[i];
    sx  += xi;
    sy  += yi;
    sxx += xi * xi;
    sxy += xi * yi;
  }
  double denom = (n * sxx - sx * sx);
  if (Math.abs(denom) < 1e-12) {
    // vertical-ish; fallback to constant
    double mean = sy / n;
    return new FitResult(0.0, mean);
  }
  double m = (n * sxy - sx * sy) / denom;
  double b = (sy - m * sx) / n;
  return new FitResult(m, b);
}
```

## MaxAbsError

```java
double maxAbsError(float[] y, int s, int e, double m, double b) {
  double mx = 0.0;
  for (int i = s; i < e; i++) {
    double pred = m * i + b;
    double err = Math.abs(pred - y[i]);
    if (err > mx) mx = err;
  }
  return mx;
}
```

## PWL Decompress

```java
float[] pwlDecompress(ArrayList<Segment> segs, int length) {
  float[] out = new float[length];
  for (Segment g : segs) {
    for (int i = g.start; i < g.end; i++) {
      double v = g.m * i + g.b;
      out[i] = (float)v;
    }
  }
  return out;
}
```



