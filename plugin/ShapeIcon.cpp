#include "ShapeIcon.h"

namespace eq1
{

staple::Icon shapeIcon (Shape shape)
{
    using staple::Icon;
    switch (shape)
    {
        case Shape::Bell: return Icon::bell;
        case Shape::LowShelf: return Icon::lowShelf;
        case Shape::LowCut: return Icon::lowCut;
        case Shape::HighShelf: return Icon::highShelf;
        case Shape::HighCut: return Icon::highCut;
        case Shape::Notch: return Icon::notch;
        case Shape::BandPass: return Icon::bandPass;
        case Shape::TiltShelf: return Icon::tiltShelf;
        case Shape::FlatTilt: return Icon::flatTilt;
        case Shape::AllPass: return Icon::allPass;
    }
    return Icon::bell;
}

} // namespace eq1
