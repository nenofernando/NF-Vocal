#include "Plugin.h"
#include <BinaryData.h>
using namespace juce;
static const StringArray ids{"input","bass","treble","compress","reverb","delay","pitch","output"};
static const StringArray names{"INPUT","BASS","TREBLE","COMPRESS","REVERB","DELAY","PITCH","OUTPUT"};
static const StringArray modeIds{"bassMode","trebleMode","compressMode","reverbMode","delayMode","pitchMode"};
static const std::array<StringArray,6> modes{{{"SUB","LOWER","UPPER"},{"BITE","TOP","ROOF"},{"PUSH","SPANK","WALL"},{"TIGHT","LARGE","CHAMBER"},{"SLAP","EIGHTH","QUARTER"},{"STEREO","WIDE","SPREADER"}}};
AudioProcessorValueTreeState::ParameterLayout NFVocal::layout(){
 AudioProcessorValueTreeState::ParameterLayout l;
 for(int i=0;i<8;++i) l.add(std::make_unique<AudioParameterFloat>(ParameterID{ids[i],1},names[i],NormalisableRange<float>(i==0||i==7?-24.f:-10.f,i==0||i==7?24.f:10.f,0.01f),0.f));
 for(int i=0;i<6;++i) l.add(std::make_unique<AudioParameterChoice>(ParameterID{modeIds[i],1},names[i+1]+" MODE",modes[i],0));
 for(int i=1;i<7;++i) l.add(std::make_unique<AudioParameterBool>(ParameterID{ids[i]+"On",1},names[i]+" ON",true));
 l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass",1},"Bypass",false));return l;
}
NFVocal::NFVocal():AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true).withOutput("Output",AudioChannelSet::stereo(),true)),state(*this,nullptr,"NFVocal",layout()){
 for(int i=0;i<8;++i)params[i]=state.getRawParameterValue(ids[i]);
 for(int i=0;i<6;++i){modeParams[i]=state.getRawParameterValue(modeIds[i]);onParams[i]=state.getRawParameterValue(ids[i+1]+"On");}
 bypassParam=state.getRawParameterValue("bypass");
}
bool NFVocal::isBusesLayoutSupported(const BusesLayout& l)const{return (l.getMainInputChannelSet()==AudioChannelSet::mono()||l.getMainInputChannelSet()==AudioChannelSet::stereo())&&(l.getMainOutputChannelSet()==l.getMainInputChannelSet()||l.getMainOutputChannelSet()==AudioChannelSet::stereo());}
void NFVocal::Biquad::shelf(double fs,float f,float db,bool high){
 double A=std::pow(10.,db/40.),w=2*MathConstants<double>::pi*jmin(double(f),fs*.45)/fs,c=std::cos(w),s=std::sin(w),alpha=s/std::sqrt(2.),q=2*std::sqrt(A)*alpha;
 double B0,B1,B2,A0,A1,A2;
 if(high){B0=A*((A+1)+(A-1)*c+q); B1=-2*A*((A-1)+(A+1)*c);B2=A*((A+1)+(A-1)*c-q);A0=(A+1)-(A-1)*c+q;A1=2*((A-1)-(A+1)*c);A2=(A+1)-(A-1)*c-q;}
 else{B0=A*((A+1)-(A-1)*c+q);B1=2*A*((A-1)-(A+1)*c);B2=A*((A+1)-(A-1)*c-q);A0=(A+1)+(A-1)*c+q;A1=-2*((A-1)+(A+1)*c);A2=(A+1)+(A-1)*c-q;}
 b0=float(B0/A0);b1=float(B1/A0);b2=float(B2/A0);a1=float(A1/A0);a2=float(A2/A0);
}
void NFVocal::prepareToPlay(double rate,int){sr=rate;pos=ppos=0;env=phase=0;controlTick=0;bassMode=trebleMode=reverbMode=-1;lastBassDb=lastTrebleDb=999;
 attackCoeff=float(std::exp(-1./(sr*.003)));releaseCoeff=float(std::exp(-1./(sr*.09)));for(int c=0;c<2;++c){delay[c].assign(size_t(sr*3)+8,0);pitch[c].assign(size_t(sr*.15)+8,0);bass[c]=Biquad{};treble[c]=Biquad{};dlp[c]=0;}for(int i=0;i<8;++i){smooth[i].reset(sr,.03);smooth[i].setCurrentAndTargetValue(value(ids[i]));}const float targets[4]={Decibels::decibelsToGain(value("input")),Decibels::decibelsToGain(value("output")),Decibels::decibelsToGain(-10.f-25.f*(value("compress")+10.f)*.05f),Decibels::decibelsToGain((value("compress")+10.f)*.35f)};
 for(int i=0;i<4;++i){gainSmooth[i].reset(sr,.03);gainSmooth[i].setCurrentAndTargetValue(targets[i]);}
 reverb.setSampleRate(sr);reverb.reset();}
float NFVocal::read(const std::vector<float>& b,int p,float d)const{float ix=float(p)-d;if(ix<0)ix+=float(b.size());if(ix>=float(b.size()))ix=0;int a=int(ix);float f=ix-float(a);int next=a+1;if(next==int(b.size()))next=0;return b[size_t(a)]*(1-f)+b[size_t(next)]*f;}
void NFVocal::processBlock(AudioBuffer<float>& b,MidiBuffer&){
 ScopedNoDenormals nd;
 const int n=b.getNumSamples(),nc=b.getNumChannels();
 if(nc<1||n==0||delay[0].empty())return;
 if(getTotalNumInputChannels()==1&&nc>1)b.copyFrom(1,0,b,0,0,n);
 float inp=0,outp=0,maxgr=0;
 for(int c=0;c<nc;++c)inp=jmax(inp,b.getMagnitude(c,0,n));
 if(bypassParam->load()>.5f){inMeter=Decibels::gainToDecibels(inp,-100.f);outMeter=inMeter.load();reduction=0;return;}
 int m[6];bool on[6];
 for(int i=0;i<6;++i){m[i]=int(modeParams[i]->load());on[i]=onParams[i]->load()>.5f;}
 float target[8];for(int i=0;i<8;++i){target[i]=params[i]->load();smooth[i].setTargetValue(target[i]);}
 gainSmooth[0].setTargetValue(Decibels::decibelsToGain(target[0]));
 gainSmooth[1].setTargetValue(Decibels::decibelsToGain(target[7]));
 const float amount=(target[3]+10.f)*.05f;
 gainSmooth[2].setTargetValue(Decibels::decibelsToGain(-10.f-25.f*amount));
 gainSmooth[3].setTargetValue(Decibels::decibelsToGain(amount*7.f));
 double bpm=120;
 if(auto* ph=getPlayHead())if(auto p=ph->getPosition())if(auto t=p->getBpm())bpm=jlimit(30.,300.,*t);
 const float dt=float(sr*(m[4]==0?.095:60./bpm*(m[4]==1?.5:1.)));
 const float rightOffset=float(sr*.009),window=float(sr*.045),pitchBase=float(sr*.012);
 const float cents=m[5]==0?6.f:m[5]==1?10.f:16.f;
 const float phaseStep=float((std::pow(2.,cents/1200.)-1)/window);
 const float compExponent=m[2]==0?-.5f:m[2]==1?-.75f:-.9f;
 const float feedback=m[4]==0?.12f:.32f;
 if(reverbMode!=m[3]){Reverb::Parameters rp;rp.roomSize=m[3]==0?.22f:m[3]==1?.78f:.55f;rp.damping=.55f;rp.wetLevel=1;rp.dryLevel=0;rp.width=1;reverb.setParameters(rp);reverbMode=m[3];}
 // Fixed chunks keep all temporary audio on the stack, including host blocks
 // larger than the size announced in prepareToPlay. No audio-thread allocation.
 constexpr int chunkSize=256;
 float dry[2][chunkSize],wet[2][chunkSize],mix[3][chunkSize],outputGain[chunkSize];
 for(int offset=0;offset<n;offset+=chunkSize){
 const int count=jmin(chunkSize,n-offset);
 for(int s=0;s<count;++s){
 float v[8];for(int i=0;i<8;++i)v[i]=smooth[i].getNextValue();
 if(controlTick==0){
 const float lowDb=on[0]?v[1]*1.2f:0.f,highDb=on[1]?v[2]*1.2f:0.f;
 if(std::abs(lowDb-lastBassDb)>.0001f||m[0]!=bassMode){bass[0].shelf(sr,m[0]==0?100.f:m[0]==1?220.f:400.f,lowDb,false);bass[1].copyCoefficients(bass[0]);lastBassDb=lowDb;bassMode=m[0];}
 if(std::abs(highDb-lastTrebleDb)>.0001f||m[1]!=trebleMode){treble[0].shelf(sr,m[1]==0?3000.f:m[1]==1?6500.f:11000.f,highDb,true);treble[1].copyCoefficients(treble[0]);lastTrebleDb=highDb;trebleMode=m[1];}
 }
 controlTick=(controlTick+1)&63;
 const float inputGain=gainSmooth[0].getNextValue();
 float x[2];for(int c=0;c<2;++c)x[c]=treble[c].tick(bass[c].tick(b.getSample(jmin(c,nc-1),offset+s)*inputGain));
 const float peak=jmax(std::abs(x[0]),std::abs(x[1]));
 const float coeff=peak>env?attackCoeff:releaseCoeff;env=coeff*env+(1-coeff)*peak;
 const float threshold=gainSmooth[2].getNextValue(),makeup=gainSmooth[3].getNextValue();
 const float compression=on[2]&&env>threshold?std::pow(env/threshold,compExponent):1.f;
 if(on[2])maxgr=jmax(maxgr,1.f-compression);
 const float gain=on[2]?compression*makeup:1.f;
 for(int c=0;c<2;++c)wet[c][s]=dry[c][s]=x[c]*gain;
 mix[0][s]=on[3]?(v[4]+10.f)*.0275f:0.f;
 mix[1][s]=on[4]?(v[5]+10.f)*.025f:0.f;
 mix[2][s]=on[5]?(v[6]+10.f)*.025f:0.f;
 outputGain[s]=gainSmooth[1].getNextValue();
 }
 reverb.processStereo(wet[0],wet[1],count);
 for(int s=0;s<count;++s){
 phase+=phaseStep;if(phase>=1.f)phase-=1.f;
 // The two half-window crossfades share their cosine and run in opposition.
 const float w=.5f-.5f*std::cos(MathConstants<float>::twoPi*phase);
 for(int c=0;c<2;++c){
 const float d=read(delay[c],pos,dt+(c==1?rightOffset:0));dlp[c]+=.24f*(d-dlp[c]);delay[c][size_t(pos)]=dry[c][s]+dlp[c]*feedback;
 pitch[c][size_t(ppos)]=dry[c][s];float shifted=0;
 if(mix[2][s]>0){float ph=c==0?phase:1.f-phase;if(ph>=1.f)ph-=1.f;float ph2=ph+.5f;if(ph2>=1.f)ph2-=1.f;
 shifted=read(pitch[c],ppos,pitchBase+ph*window)*w+read(pitch[c],ppos,pitchBase+ph2*window)*(1.f-w);}
 float y=(dry[c][s]+mix[0][s]*wet[c][s]+mix[1][s]*dlp[c]+mix[2][s]*shifted)*outputGain[s];
 if(!std::isfinite(y))y=0;
 if(c<nc){b.setSample(c,offset+s,y);outp=jmax(outp,std::abs(y));}
 }
 if(++pos==int(delay[0].size()))pos=0;if(++ppos==int(pitch[0].size()))ppos=0;
 }
 }
 inMeter=Decibels::gainToDecibels(inp,-100.f);outMeter=Decibels::gainToDecibels(outp,-100.f);
 reduction=-Decibels::gainToDecibels(jmax(.00001f,1.f-maxgr),-100.f);
}
void NFVocal::getStateInformation(MemoryBlock& b){auto tree=state.copyState();tree.setProperty("editorWidth",editorWidth.load(),nullptr);auto xml=tree.createXml();copyXmlToBinary(*xml,b);}
void NFVocal::setStateInformation(const void* d,int n){if(auto x=getXmlFromBinary(d,n))if(x->hasTagName(state.state.getType())){auto tree=ValueTree::fromXml(*x);editorWidth=jlimit(650,1400,int(tree.getProperty("editorWidth",800)));state.replaceState(tree);}}
void NFVocal::preset(int p){
 static const float presets[6][8]={{0,0,0,-10,-10,-10,-10,0},{0,-1,2,1,-5,-7,-7,-2},{0,1,3,4,-7,-9,-6,-3},{0,-2,1,2,-1,-4,-2,-3},{0,2,-1,3,-6,-3,-8,-3},{0,-1,4,5,-8,-10,-9,-4}};
 for(int i=0;i<8;++i)set(ids[i],presets[jlimit(0,5,p)][i]);for(int i=0;i<6;++i){set(modeIds[i],p==3?1.f:0.f);set(ids[i+1]+"On",1);}set("bypass",0);
}
class FaderLook:public LookAndFeel_V4{
public:
 Slider::SliderLayout getSliderLayout(Slider& slider)override{
 Slider::SliderLayout result;const int w=slider.getWidth(),h=slider.getHeight();
 result.sliderBounds=Rectangle<int>(0,26,w,jmax(1,h-86));
 result.textBoxBounds=Rectangle<int>(4,h-24,w-8,22);return result;
 }
 void drawLinearSlider(Graphics& g,int x,int y,int w,int h,float p,float,float,Slider::SliderStyle,Slider&)override{
 g.setColour(Colours::black);g.fillRoundedRectangle(float(x+w/2-4),float(y),8,float(h),4);g.setColour(Colour(0xff55565b));g.drawLine(float(x+w/2+5),float(y),float(x+w/2+5),float(y+h));
 for(int i=0;i<9;++i){float yy=float(y)+float(h)*float(i)/8;g.setColour(Colour(0xff5c5c64));g.drawHorizontalLine(int(yy),float(x+4),float(x+w/2-12));}
 Rectangle<float> r(float(x+w/2-19),p-23,38,46);g.setGradientFill(ColourGradient(Colour(0xffeeeeef),0,r.getY(),Colour(0xff6c6c75),0,r.getBottom(),false));g.fillRoundedRectangle(r,5);g.setColour(Colour(0xff111116));g.drawRoundedRectangle(r,5,1.5f);g.drawHorizontalLine(int(p),r.getX()+3,r.getRight()-3);}
};
class Editor: public AudioProcessorEditor,private Timer{
 NFVocal& p;FaderLook look;Image logo;bool ready=false;float lastIn=-1000,lastOut=-1000,lastGr=-1000;
 std::array<Slider,8> f;std::array<ComboBox,6> mode;std::array<ToggleButton,6> on;
 std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> sa;
 std::vector<std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment>> ca;
 std::vector<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>> ba;
 ToggleButton bypass{"BYPASS"};ComboBox presets;TextButton ab{"A / B"},copy{"COPY A > B"},save{"SAVE"},load{"LOAD"};MemoryBlock a,b;bool isB=false;std::unique_ptr<FileChooser> chooser;
 void timerCallback()override{
 const float scale=float(getWidth())/1000.f;
 auto refresh=[this,scale](Rectangle<float> r){repaint((r*scale).getSmallestIntegerContainer());};
 float in=p.inMeter.load(),out=p.outMeter.load(),gr=p.reduction.load();
 if(std::abs(in-lastIn)>.1f){lastIn=in;refresh({117,261,53,257});}
 if(std::abs(out-lastOut)>.1f){lastOut=out;refresh({836,261,53,257});}
 if(std::abs(gr-lastGr)>.1f){lastGr=gr;refresh({378,576,244,24});}
}
 void meter(Graphics& g,int x,float db){g.setColour(Colours::black);g.fillRoundedRectangle(float(x),263,22,229,3);for(int i=0;i<32;++i){g.setColour(db> -60+float(i)*2?(i>28?Colour(0xffff5c6b):i>23?Colour(0xffe9c76c):Colour(0xffa878ff)):Colour(0xff29262e));g.fillRect(x+4,484-i*7,14,4);}g.setColour(Colours::silver);g.setFont(11);g.drawText(String(db,1),x-13,498,50,18,Justification::centred);}
public:
 Editor(NFVocal& proc):AudioProcessorEditor(proc),p(proc){look.setColour(ComboBox::backgroundColourId,Colour(0xff111116));look.setColour(ComboBox::outlineColourId,Colour(0xff49414f));look.setColour(ComboBox::textColourId,Colour(0xffd3b3ff));look.setColour(TextButton::buttonColourId,Colour(0xff27232f));look.setColour(ToggleButton::tickColourId,Colour(0xffb782ff));look.setColour(PopupMenu::backgroundColourId,Colour(0xff17141e));setLookAndFeel(&look);setSize(p.editorWidth.load(),int(std::lround(p.editorWidth.load()*.61)));logo=ImageCache::getFromMemory(BinaryData::Logo_png,BinaryData::Logo_pngSize).rescaled(200,108,Graphics::highResamplingQuality);
 for(int i=0;i<8;++i){addAndMakeVisible(f[i]);f[i].setSliderStyle(Slider::LinearVertical);f[i].setTextBoxStyle(Slider::TextBoxBelow,false,64,22);f[i].setColour(Slider::textBoxTextColourId,Colour(0xffe2d7f5));f[i].setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);f[i].setDoubleClickReturnValue(true,0);sa.push_back(std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],f[i]));}
 for(int i=0;i<6;++i){addAndMakeVisible(mode[i]);mode[i].addItemList(modes[i],1);mode[i].setJustificationType(Justification::centred);ca.push_back(std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,modeIds[i],mode[i]));addAndMakeVisible(on[i]);on[i].setButtonText("ON");ba.push_back(std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state,ids[i+1]+"On",on[i]));}
 for(Component* c:std::initializer_list<Component*>{&bypass,&presets,&ab,&copy,&save,&load})addAndMakeVisible(c);
 ba.push_back(std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",bypass));presets.addItemList({"01 - Clean / Reset","02 - Pop Lead","03 - Rap Forward","04 - Airy Wide","05 - Warm Vintage","06 - Intimate"},1);presets.setTextWhenNothingSelected("Factory presets");presets.onChange=[this]{const int selected=presets.getSelectedId();if(selected>0)p.preset(selected-1);};
 p.getStateInformation(a);b=a;
 ab.onClick=[this]{p.getStateInformation(isB?b:a);isB=!isB;auto& data=isB?b:a;p.setStateInformation(data.getData(),int(data.getSize()));ab.setButtonText(isB?"B ACTIVE":"A ACTIVE");};copy.onClick=[this]{p.getStateInformation(isB?a:b);};
 save.onClick=[this]{chooser=std::make_unique<FileChooser>("Salvar preset NF Vocal",File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("Vocal.nfvocal"),"*.nfvocal");chooser->launchAsync(FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting,[this](const FileChooser& fc){auto file=fc.getResult();if(file!=File{}){MemoryBlock data;p.getStateInformation(data);file.replaceWithData(data.getData(),data.getSize());}});};
 load.onClick=[this]{chooser=std::make_unique<FileChooser>("Abrir preset NF Vocal",File{},"*.nfvocal");chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles,[this](const FileChooser& fc){MemoryBlock d;if(fc.getResult().loadFileAsData(d))p.setStateInformation(d.getData(),int(d.getSize()));});};setResizable(true,true);setResizeLimits(650,397,1400,854);getConstrainer()->setFixedAspectRatio(1000.0/610.0);ready=true;startTimerHz(12);resized();}
 ~Editor()override{setLookAndFeel(nullptr);}
 void mouseDoubleClick(const MouseEvent& e)override{auto pt=e.position/(float(getWidth())/1000.f);if(Rectangle<float>(62,90,120,68).contains(pt))setSize(800,488);}
 void resized()override{if(ready)p.editorWidth=getWidth();auto transform=AffineTransform::scale(float(getWidth())/1000.f);for(auto& c:f)c.setTransform(transform);for(auto& c:mode)c.setTransform(transform);for(auto& c:on)c.setTransform(transform);for(Component* c:std::initializer_list<Component*>{&bypass,&presets,&ab,&copy,&save,&load})c->setTransform(transform);presets.setBounds(25,18,265,28);ab.setBounds(305,18,90,28);copy.setBounds(405,18,115,28);save.setBounds(535,18,65,28);load.setBounds(610,18,65,28);bypass.setBounds(860,18,110,28);
 for(int i=0;i<8;++i){int x=i==0?43:i==7?883:174+(i-1)*112;f[i].setBounds(x,278,72,261);if(i>0&&i<7){mode[i-1].setBounds(x-7,203,87,27);on[i-1].setBounds(x+7,242,60,25);}}}
 void paint(Graphics& g)override{
 g.fillAll(Colour(0xff0a0a0d));g.addTransform(AffineTransform::scale(float(getWidth())/1000.f));g.setGradientFill(ColourGradient(Colour(0xff34343c),0,70,Colour(0xff101014),0,185,false));g.fillRoundedRectangle(12,63,976,122,12);
 g.setColour(Colour(0xffad75ff));g.fillRect(26,178,948,2);g.drawImageWithin(logo,68,96,100,54,RectanglePlacement::centred);
 g.setColour(Colour(0xffeeeeef));g.setFont(Font(FontOptions(42,Font::bold)));g.drawText("NF VOCAL",240,83,520,55,Justification::centred);g.setFont(12);g.setColour(Colour(0xffa8a4b2));g.drawText("SIGNATURE VOCAL PROCESSOR",275,140,450,18,Justification::centred);g.drawText("NF AUDIO TOOLS  /  1.0.1",763,140,200,18,Justification::centredRight);
 g.setColour(Colour(0xff1c1c23));g.fillRoundedRectangle(20,192,960,373,8);
 for(int i=0;i<8;++i){int x=i==0?43:i==7?883:174+(i-1)*112;g.setColour(Colour(0xff3a3643));g.drawVerticalLine(x-15,204,548);g.setColour(Colour(0xffd8d4df));g.setFont(12);g.drawText(names[i],x-12,540,96,20,Justification::centred);}
 meter(g,131,p.inMeter);meter(g,850,p.outMeter);g.setColour(Colour(0xffa878ff));g.setFont(12);g.drawText("GAIN REDUCTION  "+String(p.reduction.load(),1)+" dB",380,578,240,18,Justification::centred);g.setColour(Colour(0xff77737f));g.drawText("INPUT SENS",30,245,98,20,Justification::centred);g.drawText("OUTPUT",874,245,95,20,Justification::centred);g.drawText("FADER: 2 CLICKS = ZERO",25,578,230,18,Justification::centredLeft);g.drawText("LOGO: 2 CLICKS = DEFAULT SIZE",713,578,245,18,Justification::centredRight);
 }
};
AudioProcessorEditor* NFVocal::createEditor(){return new Editor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new NFVocal();}
