// FastNoiseSIMD_internal.h
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

#ifndef SIMD_LEVEL_H
#error Don't include this file without defining SIMD_LEVEL_H
#else
namespace FastNoiseSIMD_internal
{
	class FASTNOISE_SIMD_CLASS(SIMD_LEVEL_H) : public FastNoiseSIMD
	{
	public:
		FASTNOISE_SIMD_CLASS(SIMD_LEVEL_H)(int seed = 1337);

		float* GetEmptySet(int size) override;

		void FillWhiteNoiseSet(float* floatSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) override;

		void FillValueSet(float* floatSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) override;
		void FillValueFractalSet(float* floatSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) override;

		void FillGradientSet(float* floatSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) override;
		void FillGradientFractalSet(float* floatSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) override;

		void FillSimplexSet(float* floatSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) override;
		void FillSimplexFractalSet(float* floatSet, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float stepDistance = 1.0f) override;
	};
}
#undef SIMD_LEVEL_H
#endif
