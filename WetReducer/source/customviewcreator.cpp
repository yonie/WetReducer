//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "customviewcreator.h"
#include "numparse.h"
#include "vstgui/uidescription/uiviewcreator.h"
#include "vstgui/lib/cbitmap.h"

using namespace VSTGUI;

namespace Yonie {

static const std::string kAttrStepCount   = "step-count";
static const std::string kAttrCoarseStep  = "coarse-step";

//------------------------------------------------------------------------
// SteppedKnobCreator
//------------------------------------------------------------------------
SteppedKnobCreator::SteppedKnobCreator()
{
    UIViewFactory::registerViewCreator(*this);
}

CView* SteppedKnobCreator::create(const UIAttributes&, const IUIDescription*) const
{
    return new SteppedKnob(CRect(0, 0, 60, 60));
}

bool SteppedKnobCreator::apply(CView* view, const UIAttributes& attributes,
                               const IUIDescription* /*description*/) const
{
    auto* knob = dynamic_cast<SteppedKnob*>(view);
    if (!knob)
        return false;

    // No need to touch the bitmap or the standard control attributes here:
    // UIViewFactory walks getBaseViewName() and applies the CAnimKnob creator
    // for us before this returns.
    int32_t steps = 0;
    if (attributes.getIntegerAttribute(kAttrStepCount, steps) && steps > 1)
        knob->setStepCount(steps);

    // How many of those steps make one ordinary detent. Absent means 1, so a
    // knob without the attribute behaves exactly as it did before Shift
    // existed: every step reachable, no modifier needed.
    int32_t coarse = 0;
    if (attributes.getIntegerAttribute(kAttrCoarseStep, coarse) && coarse > 1)
        knob->setCoarseStep(coarse);

    return true;
}

bool SteppedKnobCreator::getAttributeNames(StringList& names) const
{
    names.emplace_back(kAttrStepCount);
    names.emplace_back(kAttrCoarseStep);
    return true;
}

IViewCreator::AttrType SteppedKnobCreator::getAttributeType(const string& name) const
{
    if (name == kAttrStepCount)  return kIntegerType;
    if (name == kAttrCoarseStep) return kIntegerType;
    return kUnknownType;
}

bool SteppedKnobCreator::getAttributeValue(CView* view, const string& name,
                                           string& value, const IUIDescription*) const
{
    auto* knob = dynamic_cast<SteppedKnob*>(view);
    if (!knob)
        return false;
    if (name == kAttrStepCount)
    {
        value = std::to_string(knob->getStepCount());
        return true;
    }
    if (name == kAttrCoarseStep)
    {
        value = std::to_string(knob->getCoarseStep());
        return true;
    }
    return false;
}

// The IN / OUT meters: WetWeld's LEDMeterView and its creator, unchanged.
static const std::string kAttrNumSegments = "num-segments";
static const std::string kAttrSegmentGap  = "segment-gap";
static const std::string kAttrHorizontal  = "horizontal";
static const std::string kAttrInverted    = "inverted";
static const std::string kAttrReduction   = "reduction-scheme";
static const std::string kAttrDbMin       = "db-min";
static const std::string kAttrDbMax       = "db-max";
static const std::string kAttrThresholds  = "db-thresholds";
static const std::string kAttrZoneYellow  = "zone-yellow";
static const std::string kAttrZoneRed     = "zone-red";
static const std::string kAttrDbEncoded   = "db-encoded";

//------------------------------------------------------------------------
LEDMeterViewCreator::LEDMeterViewCreator()
{
    UIViewFactory::registerViewCreator(*this);
}

CView* LEDMeterViewCreator::create(const UIAttributes&, const IUIDescription*) const
{
    return new LEDMeterView(CRect(0, 0, 20, 200));
}

bool LEDMeterViewCreator::apply(CView* view, const UIAttributes& attributes,
                                const IUIDescription*) const
{
    auto* meter = dynamic_cast<LEDMeterView*>(view);
    if (!meter)
        return false;

    int32_t intValue;
    if (attributes.getIntegerAttribute(kAttrNumSegments, intValue))
        meter->setNumSegments(intValue);

    double doubleValue;
    if (attributes.getDoubleAttribute(kAttrSegmentGap, doubleValue))
        meter->setSegmentGap(static_cast<CCoord>(doubleValue));

    bool boolValue;
    if (attributes.getBooleanAttribute(kAttrHorizontal, boolValue))
        meter->setHorizontal(boolValue);
    if (attributes.getBooleanAttribute(kAttrInverted, boolValue))
        meter->setInverted(boolValue);
    if (attributes.getBooleanAttribute(kAttrReduction, boolValue))
        meter->setReductionScheme(boolValue);
    if (attributes.getBooleanAttribute(kAttrDbEncoded, boolValue))
        meter->setDbEncoded(boolValue);

    double lo = 0.0, hi = 0.0;
    if (attributes.getDoubleAttribute(kAttrDbMin, lo) &&
        attributes.getDoubleAttribute(kAttrDbMax, hi))
        meter->setDbRange(lo, hi);

    if (auto* spec = attributes.getAttributeValue(kAttrThresholds))
    {
        std::vector<double> db;
        const char* c = spec->c_str();
        while (*c)
        {
            double v = 0.0;
            const char* end = Yonie::parseNumber(c, v);
            if (end == c) { ++c; continue; }
            db.push_back(v);
            c = end;
        }
        meter->setDbThresholds(db);
        meter->setDbThresholdSpec(*spec);
    }

    // Both or neither: a half-specified pair would put one boundary where the
    // art says and the other where the old fraction says.
    int32_t yellow = 0, red = 0;
    if (attributes.getIntegerAttribute(kAttrZoneYellow, yellow) &&
        attributes.getIntegerAttribute(kAttrZoneRed, red))
        meter->setZones(yellow, red);

    return true;
}

bool LEDMeterViewCreator::getAttributeNames(StringList& names) const
{
    names.emplace_back(kAttrNumSegments);
    names.emplace_back(kAttrSegmentGap);
    names.emplace_back(kAttrHorizontal);
    names.emplace_back(kAttrInverted);
    names.emplace_back(kAttrReduction);
    names.emplace_back(kAttrDbMin);
    names.emplace_back(kAttrDbMax);
    names.emplace_back(kAttrThresholds);
    names.emplace_back(kAttrZoneYellow);
    names.emplace_back(kAttrZoneRed);
    names.emplace_back(kAttrDbEncoded);
    return true;
}

IViewCreator::AttrType LEDMeterViewCreator::getAttributeType(const string& name) const
{
    if (name == kAttrNumSegments) return kIntegerType;
    if (name == kAttrSegmentGap)  return kFloatType;
    if (name == kAttrHorizontal)  return kBooleanType;
    if (name == kAttrInverted)    return kBooleanType;
    if (name == kAttrReduction)   return kBooleanType;
    if (name == kAttrDbMin)       return kFloatType;
    if (name == kAttrDbMax)       return kFloatType;
    if (name == kAttrThresholds)  return kStringType;
    if (name == kAttrZoneYellow)  return kIntegerType;
    if (name == kAttrZoneRed)     return kIntegerType;
    if (name == kAttrDbEncoded)   return kBooleanType;
    return kUnknownType;
}

bool LEDMeterViewCreator::getAttributeValue(CView* view, const string& name,
                                            string& value, const IUIDescription*) const
{
    auto* meter = dynamic_cast<LEDMeterView*>(view);
    if (!meter)
        return false;
    if (name == kAttrNumSegments) { value = "12";    return true; }
    if (name == kAttrSegmentGap)  { value = "2";     return true; }
    if (name == kAttrHorizontal)  { value = "false"; return true; }
    if (name == kAttrInverted)    { value = meter->isInverted() ? "true" : "false"; return true; }
    if (name == kAttrReduction)   { value = meter->isReductionScheme() ? "true" : "false"; return true; }
    if (name == kAttrDbMin)       { value = std::to_string(meter->dbMin()); return true; }
    if (name == kAttrDbMax)       { value = std::to_string(meter->dbMax()); return true; }
    if (name == kAttrThresholds)  { value = meter->dbThresholdSpec(); return true; }
    if (name == kAttrZoneYellow)  { value = std::to_string(meter->firstYellowSegment()); return true; }
    if (name == kAttrZoneRed)     { value = std::to_string(meter->firstRedSegment()); return true; }
    if (name == kAttrDbEncoded)   { value = meter->isDbEncoded() ? "true" : "false"; return true; }
    return false;
}


//------------------------------------------------------------------------
void registerCustomViews()
{
    // Static so each creator registers exactly once, however many editor
    // windows the host opens.
    static SteppedKnobCreator steppedKnobCreator;
    static LEDMeterViewCreator ledMeterCreator;
}

//------------------------------------------------------------------------
} // namespace Yonie
