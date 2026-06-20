#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <set>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <cmath>

#include "../include/ast_nodes.h"
#include "ast_io.h"
#include "logging_code.h"
#include "infix_parsing_code.h"

inline void collectCoeffNodes(Node* node, std::vector<Node*>& coeffs)
{
    if (node == nullptr)
        return;

    if (node->isCoeffNode())
    {
        coeffs.push_back(node);
        return;
    }

    collectCoeffNodes(node->getLeftChild(), coeffs);
    collectCoeffNodes(node->getRightChild(), coeffs);
}

inline std::vector<Node*> extractCoeffNodes(Node* root)
{
    std::vector<Node*> coeffs;
    collectCoeffNodes(root, coeffs);
    return coeffs;
}

inline double scoreMSE(
    Node* n,
    int vi,
    float* data,
    int numFloats)
{
    NodeStats ns;
    computeScore(vi, 0.0, data, numFloats, n, &ns);
    return ns.mse;
}

inline double optimizeCoeffSubset_HillClimbing_Search(
    Node* n,
    const std::vector<Node*>& coeffs,
    int lo,
    int hi,
    int vi,
    float* data,
    int numFloats,
    double initialStep,
    double tolerance,
    int maxIterations)
{
    int count = hi - lo;

    if (count <= 0)
        return scoreMSE(n, vi, data, numFloats);

    double bestMSE = scoreMSE(n, vi, data, numFloats);

    std::vector<double> step(count, initialStep);

    // Determine initial sign for this subset
    for (int i = 0; i < count; i++)
    {
        Node* c = coeffs[lo + i];

        double original = c->getNodeCoeff();

        c->setNodeCoeff(original + initialStep);
        double plusMSE = scoreMSE(n, vi, data, numFloats);

        c->setNodeCoeff(original - initialStep);
        double minusMSE = scoreMSE(n, vi, data, numFloats);

        c->setNodeCoeff(original);

        if (plusMSE < bestMSE && plusMSE <= minusMSE)
            step[i] = fabs(initialStep);
        else if (minusMSE < bestMSE)
            step[i] = -fabs(initialStep);
        else
            step[i] = 0.0;
    }

    // Hill-climb this subset
    for (int iter = 0; iter < maxIterations; iter++)
    {
        double maxStep = 0.0;

        for (int i = 0; i < count; i++)
            maxStep = std::max(maxStep, fabs(step[i]));

        if (maxStep < tolerance)
            break;

        std::vector<double> oldValues(count);

        for (int i = 0; i < count; i++)
        {
            Node* c = coeffs[lo + i];

            oldValues[i] = c->getNodeCoeff();
            c->setNodeCoeff(oldValues[i] + step[i]);
        }

        double candidateMSE = scoreMSE(n, vi, data, numFloats);

        if (candidateMSE < bestMSE)
        {
            bestMSE = candidateMSE;

            for (int i = 0; i < count; i++)
                step[i] *= 2.0;
        }
        else
        {
            for (int i = 0; i < count; i++)
                coeffs[lo + i]->setNodeCoeff(oldValues[i]);

            for (int i = 0; i < count; i++)
                step[i] *= 0.5;
        }
    }

    // Recursive half search
    if (count > 1)
    {
        int mid = lo + count / 2;

        double beforeLeft = scoreMSE(n, vi, data, numFloats);
        double leftMSE = optimizeCoeffSubset_HillClimbing_Search(
            n, coeffs, lo, mid,
            vi, data, numFloats,
            initialStep, tolerance, maxIterations);

        double beforeRight = scoreMSE(n, vi, data, numFloats);
        double rightMSE = optimizeCoeffSubset_HillClimbing_Search(
            n, coeffs, mid, hi,
            vi, data, numFloats,
            initialStep, tolerance, maxIterations);

        bestMSE = std::min(bestMSE, leftMSE);
        bestMSE = std::min(bestMSE, rightMSE);
        bestMSE = std::min(bestMSE, beforeLeft);
        bestMSE = std::min(bestMSE, beforeRight);
    }

    return bestMSE;
}

inline double optimize_NodeCoeffs_HillClimbing_Search(
    Node* n,
    int vi,
    float* data,
    int numFloats,
    double initialStep = 1.0,
    double tolerance = 1.0e-6,
    int maxIterations = 1000)
{
    if (n == nullptr || data == nullptr || numFloats <= 0)
        return 1.0e99;

    std::vector<Node*> coeffs = extractCoeffNodes(n);

    if (coeffs.empty())
        return scoreMSE(n, vi, data, numFloats);

    return optimizeCoeffSubset_HillClimbing_Search(
        n,
        coeffs,
        0,
        static_cast<int>(coeffs.size()),
        vi,
        data,
        numFloats,
        initialStep,
        tolerance,
        maxIterations);
}