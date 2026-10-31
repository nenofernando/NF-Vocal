#include "Plugin.h"
#include <BinaryData.h>
using namespace juce;
static const StringArray ids{"input","bass","treble","compress","reverb","delay","pitch","output"};
static const StringArray extraIds{"deesser","mid","gate","saturator","flanger","lowCut","highCut"};
static const StringArray extraNames{"DE-ESSER","MID","GATE","SATURATOR","FLANGER","LOW-CUT","HIGH-CUT"};
static constexpr float extraDefaults[7]={0,0,0,0,0,20,20000};
static const StringArray moduleIds{"bass","mid","treble","compress","reverb","delay","pitch","deesser","gate","saturator","flanger","lowCut","highCut"};
static const StringArray moduleNames{"BASS","MID","TREBLE","COMPRESS","REVERB","DELAY","PITCH","DE-ESSER","GATE","SATURATOR","FLANGER","LOW-CUT","HIGH-CUT"};
static const StringArray midModes{"BODY","FOCUS","PRESENCE"};
static const StringArray names{"INPUT","BASS","TREBLE","COMPRESS","REVERB","DELAY","PITCH","OUTPUT"};
static const StringArray modeIds{"bassMode","trebleMode","compressMode","reverbMode","delayMode","pitchMode"};
static const std::array<StringArray,6> modes{{{"SUB","LOWER","UPPER"},{"BITE","TOP","ROOF"},{"PUSH","SPANK","WALL"},{"TIGHT","LARGE","CHAMBER"},{"SLAP","EIGHTH","QUARTER"},{"STEREO","WIDE","SPREADER"}}};
AudioProcessorValueTreeState::ParameterLayout NFVocal::layout(){
 AudioProcessorValueTreeState::ParameterLayout l;
 for(int i=0;i<8;++i) l.add(std::make_unique<AudioParameterFloat>(ParameterID{ids[i],1},names[i],NormalisableRange<float>(i==0||i==7?-24.f:-10.f,i==0||i==7?24.f:10.f,0.01f),0.f));
 for(int i=0;i<6;++i) l.add(std::make_unique<AudioParameterChoice>(ParameterID{modeIds[i],1},names[i+1]+" MODE",modes[i],0));
 for(int i=1;i<7;++i) l.add(std::make_unique<AudioParameterBool>(ParameterID{ids[i]+"On",1},names[i]+" ON",true));
 for(int i=0;i<7;++i){
 auto range=i==1?NormalisableRange<float>(-12,12,.01f):i==5?NormalisableRange<float>(20,600,1,.4f):i==6?NormalisableRange<float>(2000,20000,1,.4f):NormalisableRange<float>(0,100,.1f);
 l.add(std::make_unique<AudioParameterFloat>(ParameterID{extraIds[i],1},extraNames[i],range,extraDefaults[i]));}
 l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass",1},"Bypass",false));
 l.add(std::make_unique<AudioParameterChoice>(ParameterID{"midMode",1},"MID MODE",midModes,1));
 l.add(std::make_unique<AudioParameterBool>(ParameterID{"midOn",1},"MID ON",true));return l;
}
NFVocal::NFVocal():AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true).withOutput("Output",AudioChannelSet::stereo(),true)),state(*this,nullptr,"NFVocal",layout()){
 for(int i=0;i<8;++i)params[i]=state.getRawParameterValue(ids[i]);
 for(int i=0;i<6;++i){modeParams[i]=state.getRawParameterValue(modeIds[i]);onParams[i]=state.getRawParameterValue(ids[i+1]+"On");}
 for(int i=0;i<7;++i)extraParams[i]=state.getRawParameterValue(extraIds[i]);
 bypassParam=state.getRawParameterValue("bypass");midModeParam=state.getRawParameterValue("midMode");midOnParam=state.getRawParameterValue("midOn");setChainOrder(defaultOrder());
}
NFVocal::Order NFVocal::defaultOrder(){Order order{};for(int i=0;i<ModuleCount;++i)order[size_t(i)]=i;return order;}
NFVocal::Order NFVocal::chainOrder()const{Order order{};const auto word=routeWord.load();for(int i=0;i<ModuleCount;++i)order[size_t(i)]=int((word>>(i*4))&15);return order;}
bool NFVocal::setChainOrder(const Order& order){uint64_t word=0;unsigned used=0;for(int i=0;i<ModuleCount;++i){int id=order[size_t(i)];if(id<0||id>=ModuleCount||(used&(1u<<id)))return false;used|=1u<<id;word|=uint64_t(id)<<(i*4);}routeWord.store(word);return true;}
void NFVocal::moveModule(int module,int target){auto order=chainOrder();int source=-1;for(int i=0;i<ModuleCount;++i)if(order[size_t(i)]==module)source=i;if(source<0)return;target=jlimit(0,ModuleCount-1,target);if(source==target)return;for(int i=source;i<target;++i)order[size_t(i)]=order[size_t(i+1)];for(int i=source;i>target;--i)order[size_t(i)]=order[size_t(i-1)];order[size_t(target)]=module;if(setChainOrder(order))updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));}
bool NFVocal::isBusesLayoutSupported(const BusesLayout& l)const{return (l.getMainInputChannelSet()==AudioChannelSet::mono()||l.getMainInputChannelSet()==AudioChannelSet::stereo())&&(l.getMainOutputChannelSet()==l.getMainInputChannelSet()||l.getMainOutputChannelSet()==AudioChannelSet::stereo());}
void NFVocal::Biquad::shelf(double fs,float f,float db,bool high){
 double A=std::pow(10.,db/40.),w=2*MathConstants<double>::pi*jmin(double(f),fs*.45)/fs,c=std::cos(w),s=std::sin(w),alpha=s/std::sqrt(2.),q=2*std::sqrt(A)*alpha;
 double B0,B1,B2,A0,A1,A2;
 if(high){B0=A*((A+1)+(A-1)*c+q); B1=-2*A*((A-1)+(A+1)*c);B2=A*((A+1)+(A-1)*c-q);A0=(A+1)-(A-1)*c+q;A1=2*((A-1)-(A+1)*c);A2=(A+1)-(A-1)*c-q;}
 else{B0=A*((A+1)-(A-1)*c+q);B1=2*A*((A-1)-(A+1)*c);B2=A*((A+1)-(A-1)*c-q);A0=(A+1)+(A-1)*c+q;A1=-2*((A-1)+(A+1)*c);A2=(A+1)+(A-1)*c-q;}
 b0=float(B0/A0);b1=float(B1/A0);b2=float(B2/A0);a1=float(A1/A0);a2=float(A2/A0);
}
void NFVocal::Biquad::filter(double fs,float freq,float db,int kind){
 const double w=2*MathConstants<double>::pi*jlimit(10.,fs*.45,double(freq))/fs,c=std::cos(w),sn=std::sin(w);
 double B0,B1,B2,A0,A1,A2;
 if(kind==2){const double A=std::pow(10.,db/40.),alpha=sn/2.;B0=1+alpha*A;B1=-2*c;B2=1-alpha*A;A0=1+alpha/A;A1=-2*c;A2=1-alpha/A;}
 else{const double alpha=sn/std::sqrt(2.);B0=(1+(kind==0?c:-c))/2;B1=kind==0?-(1+c):1-c;B2=B0;A0=1+alpha;A1=-2*c;A2=1-alpha;}
 b0=float(B0/A0);b1=float(B1/A0);b2=float(B2/A0);a1=float(A1/A0);a2=float(A2/A0);
}
void NFVocal::prepareToPlay(double rate,int){sr=rate;activeRoute=routeWord.load();routeFade=0;previousOut[0]=previousOut[1]=0;
 flangePos=0;flangePhase=gateEnv=deEssEnv=0;gateGain=deEssGain=1;
 gateAttack=float(std::exp(-1./(sr*.001)));gateRelease=float(std::exp(-1./(sr*.08)));
 essAttack=float(std::exp(-1./(sr*.0005)));essRelease=float(std::exp(-1./(sr*.045)));essLowCoeff=float(1-std::exp(-2*MathConstants<double>::pi*jmin(5500.,sr*.2)/sr));
 for(int c=0;c<2;++c){flange[c].assign(size_t(sr*.02)+8,0);midEq[c]=Biquad{};lowCut[c]=Biquad{};highCut[c]=Biquad{};deEssLow[c]=0;}
 for(int i=0;i<7;++i){extraSmooth[i].reset(sr,.03);extraSmooth[i].setCurrentAndTargetValue(extraParams[i]->load());}
pos=ppos=0;env=phase=0;controlTick=0;bassMode=trebleMode=reverbMode=-1;lastBassDb=lastTrebleDb=999;
 attackCoeff=float(std::exp(-1./(sr*.003)));releaseCoeff=float(std::exp(-1./(sr*.09)));for(int c=0;c<2;++c){delay[c].assign(size_t(sr*3)+8,0);pitch[c].assign(size_t(sr*.15)+8,0);bass[c]=Biquad{};treble[c]=Biquad{};dlp[c]=0;}for(int i=0;i<8;++i){smooth[i].reset(sr,.03);smooth[i].setCurrentAndTargetValue(value(ids[i]));}const float targets[4]={Decibels::decibelsToGain(value("input")),Decibels::decibelsToGain(value("output")),Decibels::decibelsToGain(-10.f-25.f*(value("compress")+10.f)*.05f),Decibels::decibelsToGain((value("compress")+10.f)*.35f)};
 for(int i=0;i<4;++i){gainSmooth[i].reset(sr,.03);gainSmooth[i].setCurrentAndTargetValue(targets[i]);}
 reverb.setSampleRate(sr);reverb.reset();}
float NFVocal::read(const std::vector<float>& b,int p,float d)const{float ix=float(p)-d;if(ix<0)ix+=float(b.size());if(ix>=float(b.size()))ix=0;int a=int(ix);float f=ix-float(a);int next=a+1;if(next==int(b.size()))next=0;return b[size_t(a)]*(1-f)+b[size_t(next)]*f;}
void NFVocal::processBlock(AudioBuffer<float>& b,MidiBuffer&){
 ScopedNoDenormals nd;const int n=b.getNumSamples(),nc=b.getNumChannels();if(nc<1||n==0||delay[0].empty())return;
 if(getTotalNumInputChannels()==1&&nc>1)b.copyFrom(1,0,b,0,0,n);
 float inp=0,outp=0,maxgr=0;for(int c=0;c<nc;++c)inp=jmax(inp,b.getMagnitude(c,0,n));
 if(bypassParam->load()>.5f){inMeter=Decibels::gainToDecibels(inp,-100.f);outMeter=inMeter.load();reduction=0;return;}
 const auto word=routeWord.load();Order order{};for(int i=0;i<ModuleCount;++i)order[size_t(i)]=int((word>>(i*4))&15);
 if(word!=activeRoute){activeRoute=word;routeFade=128;for(int c=0;c<2;++c)routeStart[c]=previousOut[c];}
 int m[6];bool on[6];for(int i=0;i<6;++i){m[i]=int(modeParams[i]->load());on[i]=onParams[i]->load()>.5f;}
 const bool midOn=midOnParam->load()>.5f;const int midMode=int(midModeParam->load());const float midFrequency=midMode==0?400.f:midMode==1?1200.f:2600.f;
 for(int i=0;i<7;++i)extraSmooth[i].setTargetValue(extraParams[i]->load());
 float target[8];for(int i=0;i<8;++i){target[i]=params[i]->load();smooth[i].setTargetValue(target[i]);}
 gainSmooth[0].setTargetValue(Decibels::decibelsToGain(target[0]));gainSmooth[1].setTargetValue(Decibels::decibelsToGain(target[7]));
 const float amount=(target[3]+10)*.05f;gainSmooth[2].setTargetValue(Decibels::decibelsToGain(-10.f-25.f*amount));gainSmooth[3].setTargetValue(Decibels::decibelsToGain(amount*7));
 double bpm=120;if(auto* ph=getPlayHead())if(auto p=ph->getPosition())if(auto t=p->getBpm())bpm=jlimit(30.,300.,*t);
 const float dt=float(sr*(m[4]==0?.095:60./bpm*(m[4]==1?.5:1.))),rightOffset=float(sr*.009),window=float(sr*.045),pitchBase=float(sr*.012);
 const float cents=m[5]==0?6.f:m[5]==1?10.f:16.f,phaseStep=float((std::pow(2.,cents/1200.)-1)/window);
 const float compExponent=m[2]==0?-.5f:m[2]==1?-.75f:-.9f,feedback=m[4]==0?.12f:.32f;
 if(reverbMode!=m[3]){juce::Reverb::Parameters rp;rp.roomSize=m[3]==0?.22f:m[3]==1?.78f:.55f;rp.damping=.55f;rp.wetLevel=1;rp.dryLevel=0;rp.width=1;reverb.setParameters(rp);reverbMode=m[3];}
 constexpr int chunkSize=256;float audio[2][chunkSize],wet[2][chunkSize],v[8][chunkSize],ex[7][chunkSize],gains[4][chunkSize];
 for(int offset=0;offset<n;offset+=chunkSize){const int count=jmin(chunkSize,n-offset);const int tickStart=controlTick;
 for(int s=0;s<count;++s){for(int i=0;i<8;++i)v[i][s]=smooth[i].getNextValue();for(int i=0;i<7;++i)ex[i][s]=extraSmooth[i].getNextValue();for(int i=0;i<4;++i)gains[i][s]=gainSmooth[i].getNextValue();for(int c=0;c<2;++c)audio[c][s]=b.getSample(jmin(c,nc-1),offset+s)*gains[0][s];}
 // Every stage receives the output of the preceding stage. The same atomic
 // route snapshot drives the entire host block; scratch storage is bounded.
 for(int module:order){
 if(module==Reverb){for(int c=0;c<2;++c)std::copy_n(audio[c],count,wet[c]);reverb.processStereo(wet[0],wet[1],count);for(int s=0;s<count;++s)for(int c=0;c<2;++c)audio[c][s]+=wet[c][s]*(on[3]?(v[4][s]+10)*.0275f:0);continue;}
 for(int s=0;s<count;++s){const bool update=((tickStart+s)&63)==0;
 switch(module){
 case Bass:case Treble:case Mid:case LowCut:case HighCut:{
 Biquad* eq=module==Bass?bass:module==Treble?treble:module==Mid?midEq:module==LowCut?lowCut:highCut;
 if(update){if(module==Bass)eq[0].shelf(sr,m[0]==0?100.f:m[0]==1?220.f:400.f,on[0]?v[1][s]*1.2f:0,false);
 else if(module==Treble)eq[0].shelf(sr,m[1]==0?3000.f:m[1]==1?6500.f:11000.f,on[1]?v[2][s]*1.2f:0,true);
 else if(module==Mid)eq[0].filter(sr,midFrequency,midOn?ex[1][s]:0,2);
 else eq[0].filter(sr,module==LowCut?ex[5][s]:ex[6][s],0,module==LowCut?0:1);eq[1].copyCoefficients(eq[0]);}
 const bool enabled=module==LowCut?ex[5][s]>20.5f:module==HighCut?ex[6][s]<19999.5f:true;
 for(int c=0;c<2;++c){auto y=eq[c].tick(audio[c][s]);if(enabled)audio[c][s]=y;}break;}
 case Gate:{const float peak=jmax(std::abs(audio[0][s]),std::abs(audio[1][s]));const float ec=peak>gateEnv?gateAttack:gateRelease;gateEnv=ec*gateEnv+(1-ec)*peak;
 const float threshold=Decibels::decibelsToGain(-72+ex[2][s]*.54f),wanted=ex[2][s]<.01f?1.f:jlimit(0.f,1.f,gateEnv/threshold),goal=wanted*wanted*wanted;
 const float gc=goal>gateGain?gateAttack:gateRelease;gateGain=gc*gateGain+(1-gc)*goal;for(int c=0;c<2;++c)audio[c][s]*=gateGain;break;}
 case Deesser:{float high[2],peak=0;for(int c=0;c<2;++c){deEssLow[c]+=essLowCoeff*(audio[c][s]-deEssLow[c]);high[c]=audio[c][s]-deEssLow[c];peak=jmax(peak,std::abs(high[c]));}
 const float ec=peak>deEssEnv?essAttack:essRelease;deEssEnv=ec*deEssEnv+(1-ec)*peak;const float threshold=Decibels::decibelsToGain(-12-ex[0][s]*.24f),goal=deEssEnv>threshold?std::pow(threshold/jmax(deEssEnv,.000001f),.75f):1.f;
 deEssGain=ec*deEssGain+(1-ec)*goal;for(int c=0;c<2;++c)audio[c][s]-=high[c]*(1-deEssGain)*ex[0][s]*.01f;break;}
 case Saturator:{const float mix=ex[3][s]*.01f,drive=1+mix*23;
 // Stronger drive with a deliberate hard-clipping component at high amounts.
 for(int c=0;c<2;++c){const float driven=audio[c][s]*drive;const float soft=std::tanh(driven),hard=jlimit(-1.f,1.f,driven);const float bite=jlimit(0.f,1.f,(mix-.35f)/.65f);const float shaped=(soft+(hard-soft)*bite)/std::pow(drive,.28f);audio[c][s]+=mix*(shaped-audio[c][s]);}break;}
 case Flanger:{for(int c=0;c<2;++c){const float lfo=.5f+.5f*std::sin(flangePhase+c*.7f);const float delayed=read(flange[c],flangePos,float(sr)*(.0007f+.003f*lfo));flange[c][size_t(flangePos)]=audio[c][s]+delayed*.25f*(ex[4][s]*.01f);audio[c][s]=(audio[c][s]+delayed*(ex[4][s]*.007f))/(1+ex[4][s]*.003f);}if(++flangePos==int(flange[0].size()))flangePos=0;flangePhase+=MathConstants<float>::twoPi*.22f/float(sr);if(flangePhase>=MathConstants<float>::twoPi)flangePhase-=MathConstants<float>::twoPi;break;}
 case Compress:{const float peak=jmax(std::abs(audio[0][s]),std::abs(audio[1][s])),ec=peak>env?attackCoeff:releaseCoeff;env=ec*env+(1-ec)*peak;const float reductionGain=on[2]&&env>gains[2][s]?std::pow(env/gains[2][s],compExponent):1;const float gain=on[2]?reductionGain*gains[3][s]:1;if(on[2])maxgr=jmax(maxgr,1-reductionGain);for(int c=0;c<2;++c)audio[c][s]*=gain;break;}
 case Delay:{for(int c=0;c<2;++c){const float d=read(delay[c],pos,dt+(c==1?rightOffset:0));dlp[c]+=.24f*(d-dlp[c]);delay[c][size_t(pos)]=audio[c][s]+dlp[c]*feedback;audio[c][s]+=(on[4]?(v[5][s]+10)*.025f:0)*dlp[c];}if(++pos==int(delay[0].size()))pos=0;break;}
 case Pitch:{phase+=phaseStep;if(phase>=1)phase-=1;const float weight=.5f-.5f*std::cos(MathConstants<float>::twoPi*phase),mix=on[5]?(v[6][s]+10)*.025f:0;for(int c=0;c<2;++c){pitch[c][size_t(ppos)]=audio[c][s];if(mix>0){float ph=c==0?phase:1-phase;if(ph>=1)ph-=1;float ph2=ph+.5f;if(ph2>=1)ph2-=1;audio[c][s]+=mix*(read(pitch[c],ppos,pitchBase+ph*window)*weight+read(pitch[c],ppos,pitchBase+ph2*window)*(1-weight));}}if(++ppos==int(pitch[0].size()))ppos=0;break;}
 default:break;
 }
 }
 }
 controlTick=(controlTick+count)&63;
 for(int s=0;s<count;++s){const float blend=routeFade>0?1-float(routeFade)/128.f:1;for(int c=0;c<2;++c){float y=audio[c][s]*gains[1][s];if(routeFade>0)y=routeStart[c]+blend*(y-routeStart[c]);if(!std::isfinite(y))y=0;previousOut[c]=y;if(c<nc){b.setSample(c,offset+s,y);outp=jmax(outp,std::abs(y));}}if(routeFade>0)--routeFade;}
 }
 inMeter=Decibels::gainToDecibels(inp,-100.f);outMeter=Decibels::gainToDecibels(outp,-100.f);reduction=-Decibels::gainToDecibels(jmax(.00001f,1-maxgr),-100.f);
}

void NFVocal::getStateInformation(MemoryBlock& b){auto tree=state.copyState();StringArray chain;for(int id:chainOrder())chain.add(String(id));tree.setProperty("chainOrder",chain.joinIntoString(","),nullptr);tree.setProperty("editorWidth",editorWidth.load(),nullptr);auto xml=tree.createXml();copyXmlToBinary(*xml,b);}
void NFVocal::setStateInformation(const void* d,int n){if(auto x=getXmlFromBinary(d,n))if(x->hasTagName(state.state.getType())){auto tree=ValueTree::fromXml(*x);editorWidth=jlimit(780,1560,int(tree.getProperty("editorWidth",1000)));
 for(int i=0;i<7;++i)if(!tree.getChildWithProperty("id",extraIds[i]).isValid()){ValueTree param("PARAM");param.setProperty("id",extraIds[i],nullptr);param.setProperty("value",extraDefaults[i],nullptr);tree.addChild(param,-1,nullptr);}
 for(auto pair:std::initializer_list<std::pair<String,float>>{{"midMode",1},{"midOn",1}})if(!tree.getChildWithProperty("id",pair.first).isValid()){ValueTree param("PARAM");param.setProperty("id",pair.first,nullptr);param.setProperty("value",pair.second,nullptr);tree.addChild(param,-1,nullptr);}
 auto order=defaultOrder();auto list=StringArray::fromTokens(tree.getProperty("chainOrder").toString(),",","");
 if(list.size()==ModuleCount){Order parsed{};bool valid=true;for(int i=0;i<ModuleCount;++i){auto token=list[i].trim();if(token.isEmpty()||token.containsAnyOf("-+. ")||!token.containsOnly("0123456789"))valid=false;parsed[size_t(i)]=token.getIntValue();}if(valid&&setChainOrder(parsed))order=parsed;}
 setChainOrder(order);state.replaceState(tree);}}
void NFVocal::preset(int p){
 setChainOrder(defaultOrder());set("midMode",1);set("midOn",1);
 static const float presets[6][8]={{0,0,0,-10,-10,-10,-10,0},{0,-1,2,1,-5,-7,-7,-2},{0,1,3,4,-7,-9,-6,-3},{0,-2,1,2,-1,-4,-2,-3},{0,2,-1,3,-6,-3,-8,-3},{0,-1,4,5,-8,-10,-9,-4}};
 for(int i=0;i<7;++i)set(extraIds[i],extraDefaults[i]);
 for(int i=0;i<8;++i)set(ids[i],presets[jlimit(0,5,p)][i]);for(int i=0;i<6;++i){set(modeIds[i],p==3?1.f:0.f);set(ids[i+1]+"On",1);}set("bypass",0);
}
class FaderLook:public LookAndFeel_V4{
public:
 void drawButtonBackground(Graphics& g,Button& button,const Colour&,bool over,bool down)override{auto r=button.getLocalBounds().toFloat().reduced(.5f);g.setGradientFill(ColourGradient(Colour(down?0xff30220e:over?0xff41341d:0xff302a20),0,0,Colour(0xff11100d),0,r.getBottom(),false));g.fillRoundedRectangle(r,5);g.setColour(Colour(over?0xffffe79a:0xffb38a37));g.drawRoundedRectangle(r,5,1);}
 void drawRotarySlider(Graphics& g,int x,int y,int width,int height,float position,float start,float end,Slider&)override{
 auto r=Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(7);float radius=jmin(r.getWidth(),r.getHeight())*.5f;auto centre=r.getCentre();r={centre.x-radius,centre.y-radius,radius*2,radius*2};
 Path track;track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);g.setColour(Colour(0xffd3a13c));g.strokePath(track,PathStrokeType(3));
 Path arc;const float angle=start+position*(end-start);arc.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);g.setColour(Colour(0xffffd36d).withAlpha(.16f));g.strokePath(arc,PathStrokeType(8));g.setColour(Colour(0xffffdf83));g.strokePath(arc,PathStrokeType(3.5f));
 auto face=r.reduced(7);g.setGradientFill(ColourGradient(Colour(0xffffedaa),face.getTopLeft(),Colour(0xff9b681e),face.getBottomRight(),false));g.fillEllipse(face);g.setColour(Colour(0xff16151b));g.drawEllipse(face,1.5f);
 const float length=radius-12;g.setColour(Colour(0xfffff7d8));g.drawLine(centre.x+std::sin(angle)*length*.4f,centre.y-std::cos(angle)*length*.4f,centre.x+std::sin(angle)*length,centre.y-std::cos(angle)*length,2.5f);
 }
 Slider::SliderLayout getSliderLayout(Slider& slider)override{
 if(slider.getSliderStyle()==Slider::RotaryHorizontalVerticalDrag)return LookAndFeel_V4::getSliderLayout(slider);
 Slider::SliderLayout result;const int w=slider.getWidth(),h=slider.getHeight();
 result.sliderBounds=Rectangle<int>(0,26,w,jmax(1,h-86));
 result.textBoxBounds=Rectangle<int>(4,h-24,w-8,22);return result;
 }
 void drawLinearSlider(Graphics& g,int x,int y,int w,int h,float p,float,float,Slider::SliderStyle,Slider&)override{
 g.setColour(Colours::black);g.fillRoundedRectangle(float(x+w/2-4),float(y),8,float(h),4);g.setColour(Colour(0xff55565b));g.drawLine(float(x+w/2+5),float(y),float(x+w/2+5),float(y+h));
 for(int i=0;i<9;++i){float yy=float(y)+float(h)*float(i)/8;g.setColour(Colour(0xff5c5c64));g.drawHorizontalLine(int(yy),float(x+4),float(x+w/2-12));}
 Rectangle<float> r(float(x+w/2-19),p-23,38,46);g.setGradientFill(ColourGradient(Colour(0xffffedb3),0,r.getY(),Colour(0xffb57d27),0,r.getBottom(),false));g.fillRoundedRectangle(r,5);g.setColour(Colour(0xff111116));g.drawRoundedRectangle(r,5,1.5f);g.drawHorizontalLine(int(p),r.getX()+3,r.getRight()-3);}
};
class ModuleHandle:public Component{
public:
 int id=0,position=0;std::function<void(int,const MouseEvent&)> down,drag,up;
 void paint(Graphics& g)override{g.setColour(Colour(0xffffdc7e));g.setFont(10);g.drawText(String(position+1).paddedLeft('0',2),4,0,20,getHeight(),Justification::centredLeft);g.setColour(Colour(0xffffefc3));g.setFont(Font(FontOptions(11,Font::bold)));g.drawText(moduleNames[id],25,0,getWidth()-40,getHeight(),Justification::centred);g.setColour(Colour(0xffdbb352));for(int x=0;x<2;++x)for(int y=0;y<3;++y)g.fillEllipse(float(getWidth()-12+x*4),float(9+y*4),1.5f,1.5f);}
 void mouseDown(const MouseEvent& e)override{if(down)down(id,e);}void mouseDrag(const MouseEvent& e)override{if(drag)drag(id,e);}void mouseUp(const MouseEvent& e)override{if(up)up(id,e);}
};
class ModuleCard:public Component{
public:
 int id;Slider slider;ComboBox mode;ToggleButton on{"ON"};ModuleHandle handle;
 std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> sa;
 std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> ca;
 std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> ba;
 explicit ModuleCard(NFVocal& p,int module):id(module){setName("module-"+moduleIds[id]);handle.id=id;handle.setName("move-"+moduleIds[id]);handle.setMouseCursor(MouseCursor::DraggingHandCursor);addAndMakeVisible(handle);addAndMakeVisible(slider);slider.setName(moduleIds[id]);const bool fader=id<=NFVocal::Pitch;slider.setSliderStyle(fader?Slider::LinearVertical:Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(Slider::TextBoxBelow,false,100,22);slider.setColour(Slider::textBoxTextColourId,Colour(0xfff1dca7));slider.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);sa=std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.state,moduleIds[id],slider);
 if(id==NFVocal::Mid){slider.textFromValueFunction=[](double x){return String(x,1)+" dB";};slider.valueFromTextFunction=[](const String& x){return x.getDoubleValue();};}
 else if(!fader){slider.textFromValueFunction=[this](double x){return id>=NFVocal::LowCut?(x>=1000?String(x/1000,1)+" kHz":String(x,0)+" Hz"):x<.05?String("OFF"):String(x,0)+" %";};slider.valueFromTextFunction=[this](const String& text){auto x=text.getDoubleValue();return id>=NFVocal::LowCut&&text.containsIgnoreCase("k")?x*1000:x;};}slider.updateText();
 if(fader){String modeId;StringArray choices;if(id==NFVocal::Mid){modeId="midMode";choices=midModes;}else{const int old=id==NFVocal::Bass?0:id-1;modeId=modeIds[old];choices=modes[size_t(old)];}addAndMakeVisible(mode);mode.setName(modeId);mode.addItemList(choices,1);mode.setJustificationType(Justification::centred);ca=std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,modeId,mode);addAndMakeVisible(on);on.setName(moduleIds[id]+"On");ba=std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state,moduleIds[id]+"On",on);}}
 void resized()override{const int w=getWidth(),h=getHeight();handle.setBounds(4,2,w-8,32);if(id<=NFVocal::Pitch){mode.setBounds(7,38,w-14,25);on.setBounds((w-62)/2,65,62,23);slider.setBounds((w-80)/2,94,80,h-100);}else{const int size=jmin(w-12,160);slider.setBounds((w-size)/2,45,size,jmin(h-55,size+30));}}
 void paint(Graphics& g)override{g.setColour(Colour(0xff1c1b23));g.fillRoundedRectangle(getLocalBounds().toFloat(),7);g.setColour(Colour(0xff71542b));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),7,1);g.setColour(Colour(0xff9a742b));g.drawHorizontalLine(35,8,float(getWidth()-8));}
};
class Editor:public AudioProcessorEditor,private Timer{
 NFVocal& p;FaderLook look;Image logo,signature;bool ready=false;float lastIn=-1000,lastOut=-1000,lastGr=-1000;
 std::array<std::unique_ptr<ModuleCard>,NFVocal::ModuleCount> cards;NFVocal::Order shown{};
 Slider input,output;ToggleButton bypass{"BYPASS"};ComboBox presets;TextButton ab{"A / B"},copy{"COPY A > B"},save{"SAVE"},load{"LOAD"};MemoryBlock a,b;bool isB=false;std::unique_ptr<FileChooser> chooser;
 std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> sa;std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
 int dragging=-1,dropTarget=-1;bool dragged=false;String dragHint;
 float scale()const{return float(getWidth())/1120.f;}
 Rectangle<int> slotBounds(int slot)const{return slot<7?Rectangle<int>(145+slot*125,198,119,362):Rectangle<int>(30+(slot-7)*177,578,169,249);}
 int sourceSlot(int id)const{for(int i=0;i<NFVocal::ModuleCount;++i)if(shown[size_t(i)]==id)return i;return -1;}
 void showOrder(){shown=p.chainOrder();for(int i=0;i<NFVocal::ModuleCount;++i){auto& card=*cards[size_t(shown[size_t(i)])];card.handle.position=i;card.setBounds(slotBounds(i));card.handle.repaint();}repaint();}
 void timerCallback()override{if(shown!=p.chainOrder()){showOrder();return;}auto refresh=[this](Rectangle<float> r){repaint((r*scale()).getSmallestIntegerContainer());};const float in=p.inMeter.load(),out=p.outMeter.load(),gr=p.reduction.load();if(std::abs(in-lastIn)>.1f){lastIn=in;refresh({98,287,45,272});}if(std::abs(out-lastOut)>.1f){lastOut=out;refresh({1015,287,45,272});}if(std::abs(gr-lastGr)>.1f){lastGr=gr;refresh({30,897,515,38});}}
 void startDrag(int id,const MouseEvent& e){if(e.mods.isPopupMenu()){moveMenu(id);return;}dragging=id;dragged=false;dropTarget=-1;}
 void trackDrag(int id,const MouseEvent& e){if(dragging!=id||e.getDistanceFromDragStart()<4)return;dragged=true;auto pt=getLocalPoint(nullptr,e.getScreenPosition()).toFloat()/scale();if(pt.y<198||pt.y>827){dropTarget=-1;dragHint="";repaint();return;}const bool top=pt.y<570;const int first=top?0:7,last=top?6:12;int slot=first;float distance=1e9f;for(int i=first;i<=last;++i){const float d=std::abs(pt.x-slotBounds(i).getCentreX());if(d<distance){distance=d;slot=i;}}const bool after=pt.x>slotBounds(slot).getCentreX();int insertion=slot+(after?1:0);const int source=sourceSlot(id);dropTarget=jlimit(0,12,insertion-(source<insertion?1:0));dragHint=String("Soltar ")+(after?"depois de ":"antes de ")+moduleNames[shown[size_t(slot)]];repaint();}
 void finishDrag(int id,const MouseEvent&){if(dragging!=id)return;if(dragged&&dropTarget>=0)p.moveModule(id,dropTarget);dragging=dropTarget=-1;dragHint="";showOrder();}
 void moveMenu(int id){PopupMenu before,after,menu;auto current=p.chainOrder();for(int i=0;i<NFVocal::ModuleCount;++i)if(current[size_t(i)]!=id){before.addItem(100+i,moduleNames[current[size_t(i)]]);after.addItem(200+i,moduleNames[current[size_t(i)]]);}menu.addSubMenu("Mover antes de",before);menu.addSubMenu("Mover depois de",after);menu.addSeparator();menu.addItem(1,"Restaurar ordem original");Component::SafePointer<Editor> safe(this);menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&cards[size_t(id)]->handle),[safe,id,current](int result){if(!safe||result==0)return;if(result==1){safe->p.setChainOrder(NFVocal::defaultOrder());safe->p.updateHostDisplay(AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));}else{const bool after=result>=200;const int target=result-(after?200:100);auto now=safe->p.chainOrder();const int refId=current[size_t(target)];int ref=0,source=0;for(int i=0;i<NFVocal::ModuleCount;++i){if(now[size_t(i)]==refId)ref=i;if(now[size_t(i)]==id)source=i;}const int insertion=ref+(after?1:0);safe->p.moveModule(id,insertion-(source<insertion?1:0));}safe->showOrder();});}
 void meter(Graphics& g,int x,float db){g.setColour(Colours::black);g.fillRoundedRectangle(float(x),290,15,212,3);for(int i=0;i<29;++i){g.setColour(db>-60+i*2?(i>26?Colour(0xffff5c6b):i>22?Colour(0xffe9c76c):Colour(0xffffd56c)):Colour(0xff2f291f));g.fillRect(x+3,494-i*7,9,4);}g.setColour(Colour(0xffb2a8bb));g.setFont(10);g.drawText(String(db,1),x-14,538,43,17,Justification::centred);}
public:
 explicit Editor(NFVocal& proc):AudioProcessorEditor(proc),p(proc){look.setColour(ComboBox::backgroundColourId,Colour(0xff111116));look.setColour(ComboBox::outlineColourId,Colour(0xffb28a3c));look.setColour(ComboBox::textColourId,Colour(0xffffdfa0));look.setColour(TextButton::buttonColourId,Colour(0xff2c251a));look.setColour(TextButton::textColourOffId,Colour(0xffffe09b));look.setColour(TextButton::textColourOnId,Colour(0xffffedb4));look.setColour(ToggleButton::textColourId,Colour(0xffffe7b2));look.setColour(ToggleButton::tickColourId,Colour(0xffffd875));look.setColour(PopupMenu::backgroundColourId,Colour(0xff17141e));setLookAndFeel(&look);logo=ImageCache::getFromMemory(BinaryData::Logo_png,BinaryData::Logo_pngSize);signature=ImageCache::getFromMemory(BinaryData::Signature_png,BinaryData::Signature_pngSize);
 for(int i=0;i<NFVocal::ModuleCount;++i){cards[size_t(i)]=std::make_unique<ModuleCard>(p,i);auto& c=*cards[size_t(i)];addAndMakeVisible(c);c.handle.down=[this](int id,const MouseEvent& e){startDrag(id,e);};c.handle.drag=[this](int id,const MouseEvent& e){trackDrag(id,e);};c.handle.up=[this](int id,const MouseEvent& e){finishDrag(id,e);};}
 for(auto pair:std::initializer_list<std::pair<Slider*,String>>{{&input,"input"},{&output,"output"}}){auto& slider=*pair.first;addAndMakeVisible(slider);slider.setName(pair.second);slider.setSliderStyle(Slider::LinearVertical);slider.setTextBoxStyle(Slider::TextBoxBelow,false,64,22);slider.setColour(Slider::textBoxTextColourId,Colour(0xfff1dca7));slider.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);sa.push_back(std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.state,pair.second,slider));}
 for(Component* c:std::initializer_list<Component*>{&bypass,&presets,&ab,&copy,&save,&load})addAndMakeVisible(c);presets.setName("presets");bypassAttachment=std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",bypass);
 presets.addItemList({"01 - Clean / Reset","02 - Pop Lead","03 - Rap Forward","04 - Airy Wide","05 - Warm Vintage","06 - Intimate"},1);presets.setTextWhenNothingSelected("Factory presets");presets.onChange=[this]{const int selected=presets.getSelectedId();if(selected>0){p.preset(selected-1);showOrder();}};
 p.getStateInformation(a);b=a;ab.onClick=[this]{p.getStateInformation(isB?b:a);isB=!isB;auto& data=isB?b:a;p.setStateInformation(data.getData(),int(data.getSize()));ab.setButtonText(isB?"B ACTIVE":"A ACTIVE");showOrder();};copy.onClick=[this]{p.getStateInformation(isB?a:b);};
 save.onClick=[this]{chooser=std::make_unique<FileChooser>("Salvar preset NF Vocal",File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("Vocal.nfvocal"),"*.nfvocal");chooser->launchAsync(FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting,[this](const FileChooser& fc){auto file=fc.getResult();if(file!=File{}){MemoryBlock data;p.getStateInformation(data);file.replaceWithData(data.getData(),data.getSize());}});};
 load.onClick=[this]{chooser=std::make_unique<FileChooser>("Abrir preset NF Vocal",File{},"*.nfvocal");chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles,[this](const FileChooser& fc){MemoryBlock data;if(fc.getResult().loadFileAsData(data)){p.setStateInformation(data.getData(),int(data.getSize()));showOrder();}});};
 setResizable(true,true);setResizeLimits(780,655,1560,1309);getConstrainer()->setFixedAspectRatio(1120.0/940.0);setSize(jlimit(780,1560,p.editorWidth.load()),int(std::lround(jlimit(780,1560,p.editorWidth.load())*940./1120.)));ready=true;showOrder();startTimerHz(20);}
 ~Editor()override{stopTimer();setLookAndFeel(nullptr);}
 void mouseDoubleClick(const MouseEvent& e)override{auto pt=e.position/scale();if(Rectangle<float>(25,80,160,84).contains(pt))setSize(1000,839);}
 void resized()override{if(ready)p.editorWidth=getWidth();auto transform=AffineTransform::scale(scale());for(auto& c:cards)if(c)c->setTransform(transform);for(Component* c:std::initializer_list<Component*>{&input,&output,&bypass,&presets,&ab,&copy,&save,&load})c->setTransform(transform);presets.setBounds(25,18,265,28);ab.setBounds(305,18,90,28);copy.setBounds(405,18,115,28);save.setBounds(535,18,65,28);load.setBounds(610,18,65,28);bypass.setBounds(985,18,110,28);input.setBounds(29,284,72,253);output.setBounds(1051,284,60,253);if(ready)showOrder();}
 void paint(Graphics& g)override{g.fillAll(Colour(0xff0a0a0d));g.addTransform(AffineTransform::scale(scale()));g.setGradientFill(ColourGradient(Colour(0xff34343c),0,65,Colour(0xff101014),0,185,false));g.fillRoundedRectangle(12,63,1096,122,12);g.setGradientFill(ColourGradient(Colour(0xfffff1b5),32,92,Colour(0xffd19a36),32,164,false));g.drawImageWithin(logo,32,91,135,73,RectanglePlacement::centred,true);g.setColour(Colour(0xffffedb3));g.setFont(Font(FontOptions(42,Font::bold)));g.drawText("NF VOCAL",310,87,500,57,Justification::centred);g.setColour(Colour(0xffe6d3a4));g.setFont(12);g.drawText("SIGNATURE VOCAL PROCESSOR",310,144,500,20,Justification::centred);g.setColour(Colour(0xffe6b84c));g.fillRect(26,178,1068,2);
 g.setColour(Colour(0xff1b1921));g.fillRoundedRectangle(20,198,115,362,7);g.fillRoundedRectangle(1024,198,84,362,7);g.setFont(11);g.setColour(Colour(0xffecdbb5));g.drawText("INPUT",25,218,80,24,Justification::centred);g.drawText("OUTPUT",1033,218,77,24,Justification::centred);meter(g,114,p.inMeter);meter(g,1029,p.outMeter);
 if(dragging>=0&&dropTarget>=0){auto r=slotBounds(dropTarget).toFloat().expanded(3);g.setColour(Colour(0xffffdf8a));g.drawRoundedRectangle(r,9,2);}
 g.setColour(Colour(0xffffd77b));g.setFont(12);g.drawText(dragHint.isNotEmpty()?dragHint:"Arraste o nome dos modulos para mudar a ordem do audio.",30,844,780,22,Justification::centredLeft);g.setColour(Colour(0xffb1a17c));g.setFont(11);g.drawText("Botao direito no nome: mover antes / depois de outro efeito",30,870,780,18,Justification::centredLeft);g.setColour(Colour(0xffffd77b));g.drawText("GAIN REDUCTION",30,900,155,20,Justification::centredLeft);
 const float reductionDb=p.reduction.load();g.setColour(Colour(0xff17140e));g.fillRoundedRectangle(190,903,270,14,3);for(int i=0;i<36;++i){g.setColour(reductionDb>float(i)*24.f/36.f?Colour(0xffffd56c):Colour(0xff3e321e));g.fillRect(194.f+float(i)*7.28f,906.f,5.3f,8.f);}g.setFont(9);g.setColour(Colour(0xffbfb08a));for(int i=0;i<5;++i)g.drawText(String(i*6),175+i*67,920,30,13,Justification::centred);g.setFont(11);g.setColour(Colour(0xffffe19c));g.drawText(String(reductionDb,1)+" dB",470,900,80,20,Justification::centredLeft);g.setColour(Colour(0xffb1a17c));g.drawText("NF AUDIO TOOLS / 1.2.0",580,900,250,20,Justification::centredLeft);g.setGradientFill(ColourGradient(Colour(0xffffefa9),850,845,Colour(0xffd39a30),850,928,false));g.drawImageWithin(signature,850,840,245,88,RectanglePlacement::centred,true);
 }
};
AudioProcessorEditor* NFVocal::createEditor(){return new Editor(*this);}
#ifndef NFVOCAL_NO_PLUGIN_FACTORY
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new NFVocal();}
#endif
