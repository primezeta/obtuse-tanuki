// FastNoiseSIMD.h
//
// MIT License
//
// Copyright(c) 2016 Jordan Peck
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// The developer's email is jorzixdan.me2@gzixmail.com (for great email, take
// off every 'zix'.)
//

#ifndef FASTNOISE_SIMD_H
#define FASTNOISE_SIMD_H

// Comment out lines to not compile for certain instruction sets
#define FN_COMPILE_NO_SIMD_FALLBACK
#define FN_COMPILE_SSE2
#define FN_COMPILE_SSE41

// To compile FN_AVX2 set C++ code generation to use /arch:AVX(2) on FastNoiseSIMD_internal.cpp
#define FN_COMPILE_AVX2
// Note: This does not break support for pre AVX CPUs, AVX code is only run if support is detected

// Using aligned sets of memory for float arrays allows faster storing of SIMD data
// Comment out to allow unaligned float arrays to be used as sets
#define FN_ALIGNED_SETS

/*
Tested Compilers:
-MSVC v120/v140
-Intel 16.0
-GCC 5.3.1 Linux

CPU instruction support

SSE2
Intel Pentium 4 - 2001
AMD Opteron/Athlon - 2003

SEE4.1
Intel Penryn - 2007
AMD Bulldozer - Q4 2011

AVX
Intel Sandy Bridge - Q1 2011
AMD Bulldozer - Q4 2011

AVX2
Intel Haswell - Q2 2013
AMD Carrizo - Q2 2015

FMA3
Intel Haswell - Q2 2013
AMD Piledriver - 2012
*/

class FastNoiseSIMD
{
public:
	enum NoiseType { Value, ValueFractal, Gradient, GradientFractal, Simplex, SimplexFractal, WhiteNoise };
	enum FractalType { FBM, Billow, RigidMulti };

	// Creates new FastNoiseSIMD for the highest supported instuction set of the CPU 
	static FastNoiseSIMD* NewFastNoiseSIMD(int seed = 1337);

	// Returns highest detected level of CPU support
	// 3: AVX2 & FMA3
	// 2: SSE4.1
	// 1: SSE2
	// 0: Fallback, no SIMD support
	// -1: Must create a FastNoiseSIMD to detect SIMD level
	static int GetSIMDLevel(void) { return s_currentSIMDLevel; }

	// Free a noise set from memory
	static void FreeNoiseSet(float* noiseSet);


	// Returns seed used for all noise types
	int GetSeed(void) const	{ return m_seed; }

	// Sets seed used for all noise types
	// Default: 1337
	void SetSeed(int seed) { m_seed = seed; }

	// Sets frequency for all noise types
	// Default: 0.01
	void SetFrequency(float frequency) { m_frequency = frequency; }

	// Sets noise return type of (Get/Fill)NoiseSet()
	// Default: Simplex
	void SetNoiseType(NoiseType noiseType) { m_noiseType = noiseType; }


	// Sets octave count for all fractal noise types
	// Default: 3
	void SetFractalOctaves(unsigned int octaves) { m_octaves = octaves; }

	// Sets octave lacunarity for all fractal noise types
	// Default: 2.0
	void SetFractalLacunarity(float lacunarity) { m_lacunarity = lacunarity; }

	// Sets octave gain for all fractal noise types
	// Default: 0.5
	void SetFractalGain(float gain) { m_gain = gain; }

	// Sets method for combining octaves in all fractal noise types
	// Default: FBM
	void SetFractalType(FractalType fractalType) { m_fractalType = fractalType; }

	virtual float* GetEmptySet(int size) = 0;
	float* GetEmptySet(int xSize, int ySize, int zSize) { return GetEmptySet(xSize*ySize*zSize); };

	float* GetNoiseSet(int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f);
	void FillNoiseSet(float* noiseSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f);

	float* GetWhiteNoiseSet(int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f);
	virtual void FillWhiteNoiseSet(float* noiseSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) = 0;

	float* GetValueSet(int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f);
	float* GetValueFractalSet(int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f);
	virtual void FillValueSet(float* noiseSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) = 0;
	virtual void FillValueFractalSet(float* noiseSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) = 0;

	float* GetGradientSet(int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f);
	float* GetGradientFractalSet(int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f);
	virtual void FillGradientSet(float* noiseSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) = 0;
	virtual void FillGradientFractalSet(float* noiseSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) = 0;

	float* GetSimplexSet(int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f);
	float* GetSimplexFractalSet(int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f);
	virtual void FillSimplexSet(float* noiseSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) = 0;
	virtual void FillSimplexFractalSet(float* noiseSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) = 0;

protected:
	int m_seed = 0;
	float m_frequency = 0.01f;
	NoiseType m_noiseType = Simplex;

	unsigned int m_octaves = 3;
	float m_lacunarity = 2.0f;
	float m_gain = 0.5f;
	FractalType m_fractalType = FBM;

	static int s_currentSIMDLevel;
};

#define FN_NO_SIMD_FALLBACK 0
#define FN_SSE2 1
#define FN_SSE41 2
#define FN_AVX2 3

#define FASTNOISE_SIMD_CLASS2(x) FastNoiseSIMD_L##x
#define FASTNOISE_SIMD_CLASS(level) FASTNOISE_SIMD_CLASS2(level)

#endif
