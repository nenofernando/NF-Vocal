#pragma once
#include <JuceHeader.h>
#include <array>
class NFVocal : public juce::AudioProcessor {
public:
 NFVocal();
 juce::AudioProcessorValueTreeState state;
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 float value(const juce::String& id) const { return state.getRawParameterValue(id)->load(); }
 void set(const juce::String& id,float v) { auto* p=state.getParameter(id); p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture(); }
 enum Module { Bass,Mid,Treble,Compress,Reverb,Delay,Pitch,Deesser,Gate,Saturator,Flanger,LowCut,HighCut,ModuleCount };
 using Order=std::array<int,ModuleCount>;
 static Order defaultOrder();
 Order chainOrder() const;
 bool setChainOrder(const Order&);
 void moveModule(int module,int target);
 void preset(int);
 std::atomic<int> editorWidth{1000};
 std::atomic<float> inMeter{-100},outMeter{-100},reduction{0};
 void prepareToPlay(double,int) override;
 void releaseResources() override {}
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override{return true;}
 const juce::String getName() const override{return "NF Vocal";}
 bool acceptsMidi() const override{return false;} bool producesMidi() const override{return false;}
 double getTailLengthSeconds() const override{return 10;}
 int getNumPrograms() override{return 1;} int getCurrentProgram() override{return 0;}
 void setCurrentProgram(int) override{} const juce::String getProgramName(int) override{return "Default";} void changeProgramName(int,const juce::String&) override{}
 void getStateInformation(juce::MemoryBlock&) override; void setStateInformation(const void*,int) override;
 juce::AudioProcessorParameter* getBypassParameter() const override{return state.getParameter("bypass");}
private:
 struct Biquad {float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0; float tick(float x){float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;} void shelf(double,float,float,bool); void filter(double,float,float,int); void copyCoefficients(const Biquad& o){b0=o.b0;b1=o.b1;b2=o.b2;a1=o.a1;a2=o.a2;}};
 Biquad bass[2],treble[2],midEq[2],lowCut[2],highCut[2];
 std::array<juce::SmoothedValue<float>,7> extraSmooth;
 std::array<std::atomic<float>*,7> extraParams{};
 std::vector<float> flange[2];
 int flangePos=0; float flangePhase=0, gateEnv=0,gateGain=1,deEssEnv=0,deEssLow[2]={}, deEssGain=1;
 float gateAttack=0,gateRelease=0,essAttack=0,essRelease=0,essLowCoeff=0;
 std::atomic<uint64_t> routeWord{0};
 uint64_t activeRoute=0; int routeFade=0; float previousOut[2]={},routeStart[2]={};
 juce::Reverb reverb;
 std::array<juce::SmoothedValue<float>,8> smooth;
 std::array<juce::SmoothedValue<float>,4> gainSmooth;
 std::array<std::atomic<float>*,8> params{};
 std::array<std::atomic<float>*,6> modeParams{},onParams{};
 std::atomic<float>* bypassParam=nullptr;
 std::atomic<float>* midModeParam=nullptr;std::atomic<float>* midOnParam=nullptr;
 float attackCoeff=0,releaseCoeff=0,lastBassDb=999,lastTrebleDb=999;
 int controlTick=0,bassMode=-1,trebleMode=-1,reverbMode=-1;
 std::vector<float> delay[2],pitch[2];
 double sr=44100; int pos=0,ppos=0; float env=0,phase=0,dlp[2]={};
 float read(const std::vector<float>&,int,float) const;
};
