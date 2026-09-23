#include "SpectralEngine.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "base/source/fstreamer.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
const FUID processorId(0x06DEEA31,0xC7F7479C,0xB57FB2EC,0x167230A1);
const FUID controllerId(0x25494529,0x681B4A33,0xA8AA4240,0x18F71A40);
constexpr int parameterCount=9;
const double minimums[]={0,20,0,0,20,2000,-100,-24,0};
const double maximums[]={100,2000,24,36,500,20000,-30,12,1};
const double defaults[]={100,250,12,18,35,16000,-75,-6,0};
double normalized(int id,double value) {return (value-minimums[id])/(maximums[id]-minimums[id]);}
double plain(int id,double value) {return minimums[id]+value*(maximums[id]-minimums[id]);}
using Values=std::array<double,parameterCount>;
Values initial() {Values v{}; for(int i=0;i<parameterCount;++i) v[i]=normalized(i,defaults[i]); return v;}
bool readState(IBStream* stream,Values& values) {
    if(!stream) return false;
    IBStreamer reader(stream,kLittleEndian); int32 version=0;
    if(!reader.readInt32(version) || version!=1) return false;
    Values next{};
    for(auto& value:next) if(!reader.readDouble(value) || !std::isfinite(value) || value<0 || value>1) return false;
    values=next; return true;
}
class Processor final:public AudioEffect {
public:
    Processor() {setControllerClass(controllerId);}
    static FUnknown* create(void*) {return static_cast<IAudioProcessor*>(new Processor);}
    tresult PLUGIN_API initialize(FUnknown* context) override {
        auto result=AudioEffect::initialize(context); if(result!=kResultOk) return result;
        addAudioInput(STR16("Input"),SpeakerArr::kStereo); addAudioOutput(STR16("Output"),SpeakerArr::kStereo);
        return kResultOk;
    }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* ins,int32 ni,SpeakerArrangement* outs,int32 no) override {
        if(ni!=1 || no!=1 || ins[0]!=outs[0] || (ins[0]!=SpeakerArr::kMono && ins[0]!=SpeakerArr::kStereo)) return kResultFalse;
        return AudioEffect::setBusArrangements(ins,ni,outs,no);
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override {return size==kSample32?kResultTrue:kResultFalse;}
    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override {
        if(setup.sampleRate<8000 || setup.sampleRate>384000) return kResultFalse;
        auto result=AudioEffect::setupProcessing(setup);
        if(result==kResultOk) {updateSettings(); engine.prepare(setup.sampleRate);}
        return result;
    }
    tresult PLUGIN_API setActive(TBool active) override {
        if(active) {updateSettings(); engine.prepare(processSetup.sampleRate>0?processSetup.sampleRate:48000);}
        return AudioEffect::setActive(active);
    }
    uint32 PLUGIN_API getLatencySamples() override {return SpectralEngine::size;}
    uint32 PLUGIN_API getTailSamples() override {return SpectralEngine::size;}
    tresult PLUGIN_API process(ProcessData& data) override {
        // Use the final automation value in each block. Spectral gain changes
        // are smoothed at the selected response rate, not sample accurate.
        if(data.inputParameterChanges) for(int32 i=0;i<data.inputParameterChanges->getParameterCount();++i) {
            auto* queue=data.inputParameterChanges->getParameterData(i); if(!queue) continue;
            auto id=queue->getParameterId(); int32 offset=0; double value=0;
            if(id<parameterCount && queue->getPointCount()>0 && queue->getPoint(queue->getPointCount()-1,offset,value)==kResultTrue && std::isfinite(value)) values[id]=std::clamp(value,0.,1.);
        }
        updateSettings();
        if(data.numSamples<=0 || data.numInputs<1 || data.numOutputs<1) return kResultOk;
        if(data.symbolicSampleSize!=kSample32) return kResultFalse;
        auto& in=data.inputs[0]; auto& out=data.outputs[0];
        if(in.numChannels<1 || out.numChannels<1 || !in.channelBuffers32 || !out.channelBuffers32) return kResultFalse;
        bool silent=true;
        for(int32 i=0;i<data.numSamples;++i) {
            float l=(in.silenceFlags&1)?0:in.channelBuffers32[0][i];
            float r=in.numChannels>1?((in.silenceFlags&2)?0:in.channelBuffers32[1][i]):l;
            auto y=engine.tick(l,r,values[8]>=0.5);
            out.channelBuffers32[0][i]=y[0]; if(out.numChannels>1) out.channelBuffers32[1][i]=y[1];
            silent=silent && y[0]==0 && y[1]==0;
        }
        out.silenceFlags=silent?((uint64(1)<<out.numChannels)-1):0;
        return kResultOk;
    }
    tresult PLUGIN_API getState(IBStream* stream) override {
        if(!stream) return kInvalidArgument;
        IBStreamer writer(stream,kLittleEndian); if(!writer.writeInt32(1)) return kResultFalse;
        for(auto v:values) if(!writer.writeDouble(v)) return kResultFalse;
        return kResultOk;
    }
    tresult PLUGIN_API setState(IBStream* stream) override {return readState(stream,values)?kResultOk:kResultFalse;}
private:
    Values values=initial(); SpectralEngine engine;
    void updateSettings() {
        engine.settings.amount=float(plain(0,values[0])*0.01);
        engine.settings.responseMs=float(plain(1,values[1])); engine.settings.boostDb=float(plain(2,values[2]));
        engine.settings.cutDb=float(plain(3,values[3])); engine.settings.lowHz=float(plain(4,values[4]));
        engine.settings.highHz=float(plain(5,values[5])); engine.settings.floorDb=float(plain(6,values[6]));
        engine.settings.outputDb=float(plain(7,values[7]));
    }
};
class Controller final:public EditController {
public:
    static FUnknown* create(void*) {return static_cast<IEditController*>(new Controller);}
    tresult PLUGIN_API initialize(FUnknown* context) override {
        auto result=EditController::initialize(context); if(result!=kResultOk) return result;
        const TChar* names[]={STR16("Flatten"),STR16("Response"),STR16("Max boost"),STR16("Max cut"),STR16("Low rolloff"),STR16("High rolloff"),STR16("Activity floor"),STR16("Output"),STR16("Bypass")};
        const TChar* units[]={STR16("%"),STR16("ms"),STR16("dB"),STR16("dB"),STR16("Hz"),STR16("Hz"),STR16("dB"),STR16("dB"),STR16("")};
        for(int i=0;i<parameterCount;++i) {
            auto flags=ParameterInfo::kCanAutomate | (i==8?ParameterInfo::kIsBypass:0);
            auto* p=new RangeParameter(names[i],i,units[i],minimums[i],maximums[i],defaults[i],i==8?1:0,flags);
            p->setPrecision(i==2 || i==3 || i==7?1:0); parameters.addParameter(p);
        }
        return kResultOk;
    }
    tresult PLUGIN_API setComponentState(IBStream* stream) override {
        Values values{}; if(!readState(stream,values)) return kResultFalse;
        for(int i=0;i<parameterCount;++i) setParamNormalized(i,values[i]);
        return kResultOk;
    }
};
}
bool InitModule() {return true;}
bool DeinitModule() {return true;}
BEGIN_FACTORY_DEF("EvenSpectrum Audio","","")
DEF_CLASS2(INLINE_UID_FROM_FUID(processorId),PClassInfo::kManyInstances,kVstAudioEffectClass,"EvenSpectrum",Vst::kDistributable,"Fx|EQ","0.1.0",kVstVersionString,Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(controllerId),PClassInfo::kManyInstances,kVstComponentControllerClass,"EvenSpectrum Controller",0,"","0.1.0",kVstVersionString,Controller::create)
END_FACTORY
