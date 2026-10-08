#include "eq1/Response.h"

#include "ShapeDesign.h"

#include <cmath>
#include <complex>
#include <numbers>

namespace eq1
{

namespace
{
double magnitudeDb (const Cascade& cascade, double frequency, double sampleRate)
{
    const auto z = std::polar (1.0, -2.0 * std::numbers::pi * frequency / sampleRate); // z^-1
    std::complex<double> h = 1.0;
    for (int i = 0; i < cascade.count; ++i)
    {
        const auto& c = cascade.sections[static_cast<size_t> (i)];
        h *= (c.b0 + z * (c.b1 + z * c.b2)) / (1.0 + z * (c.a1 + z * c.a2));
    }
    return 20.0 * std::log10 (std::abs (h));
}

Cascade designOf (const BandSettings& band, double sampleRate)
{
    return designShape ({ structureOf (band), band.frequency, band.gain, band.q }, sampleRate);
}
} // namespace

double bandResponseDb (const BandSettings& band, double frequency, double sampleRate)
{
    return magnitudeDb (designOf (band, sampleRate), frequency, sampleRate);
}

void bandResponseDb (const BandSettings& band, const double* frequencies, double* db, int count, double sampleRate)
{
    const auto cascade = designOf (band, sampleRate);
    for (int i = 0; i < count; ++i)
        db[i] = magnitudeDb (cascade, frequencies[i], sampleRate);
}

double responseDb (const Settings& settings, double frequency, double sampleRate)
{
    double db = 0.0;
    for (const auto& band : settings.bands)
        if (band.inUse && ! band.bypass)
            db += bandResponseDb (band, frequency, sampleRate);
    return db;
}

} // namespace eq1
