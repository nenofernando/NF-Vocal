#include <CoreFoundation/CoreFoundation.h>
#include "Plugin.h"
#include <iostream>
#include <ctime>
static void require(bool ok,const char* message){if(!ok){std::cerr<<"FAIL: "<<message<<"\n";std::exit(1);}}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI gui; NFVocal p; juce::MidiBuffer midi; double energy=0;
 if(argc>1&&juce::String(argv[1])=="--benchmark"){
 p.setPlayConfigDetails(2,2,48000,256);p.prepareToPlay(48000,256);p.preset(3);juce::AudioBuffer<float> signal(2,256);
 const int blocks=10000;const auto start=std::clock();for(int block=0;block<blocks;++block){for(int c=0;c<2;++c)for(int i=0;i<256;++i)signal.setSample(c,i,.15f*std::sin(float(block*256+i)*.029f));p.processBlock(signal,midi);}double cpu=double(std::clock()-start)/CLOCKS_PER_SEC;std::cout<<"BENCH cpu_seconds="<<cpu<<" audio_seconds="<<double(blocks*256)/48000<<" percent="<<100*cpu/(double(blocks*256)/48000)<<"\n";return 0;}
 if(argc>1&&juce::String(argv[1])=="--reopen"){
 p.set("delay",3.21f);p.set("reverb",-2.34f);p.set("delayMode",2);p.set("reverbMode",1);
 for(int attempt=0;attempt<3;++attempt){std::unique_ptr<juce::AudioProcessorEditor> window(p.createEditor());CFRunLoopRunInMode(kCFRunLoopDefaultMode,.05,false);require(std::abs(p.value("delay")-3.21f)<.02f&&std::abs(p.value("reverb")+2.34f)<.02f&&p.value("delayMode")==2&&p.value("reverbMode")==1,"reopen editor preserves delay/reverb after pending UI events");}
 std::cout<<"PASS: reopen preserves delay, reverb and modes after event dispatch\n";return 0;}

 for(double rate:{44100.,48000.,96000.})for(int channels:{1,2}){
 p.setPlayConfigDetails(channels,channels,rate,256);p.prepareToPlay(rate,256);juce::AudioBuffer<float> audio(channels,256);
 for(int preset=0;preset<6;++preset){p.preset(preset);for(int block=0;block<160;++block){for(int c=0;c<channels;++c)for(int i=0;i<256;++i)audio.setSample(c,i,.15f*std::sin(float((block*256+i)*.029)));p.processBlock(audio,midi);for(int c=0;c<channels;++c)for(int i=0;i<256;++i){float v=audio.getSample(c,i);require(std::isfinite(v)&&std::abs(v)<10,"finite bounded audio");energy+=v*v;}}}
 // Stress every mode at maximum macro amount, then process silence and tails.
 for(int mode=0;mode<3;++mode){for(auto id:{"bass","treble","compress","reverb","delay","pitch"}){p.set(id,10);p.set(juce::String(id)+"Mode",float(mode));}for(int block=0;block<100;++block){audio.clear();if(block==0)audio.setSample(0,0,1);p.processBlock(audio,midi);for(int c=0;c<channels;++c)for(int i=0;i<256;++i)require(std::isfinite(audio.getSample(c,i)),"mode impulse and tails");}}
 }
 p.setPlayConfigDetails(2,2,48000,64);p.prepareToPlay(48000,64);p.preset(1);
 for(int blockSize:{1,16,64,128,257,512,1024,4096}){juce::AudioBuffer<float> buffer(2,blockSize);buffer.clear();buffer.setSample(0,0,.1f);p.processBlock(buffer,midi);for(int c=0;c<2;++c)for(int i=0;i<blockSize;++i)require(std::isfinite(buffer.getSample(c,i)),"variable host blocks including larger than prepare size");}
 p.preset(5);juce::MemoryBlock state;p.getStateInformation(state);p.set("bass",9);p.setStateInformation(state.getData(),int(state.getSize()));require(std::abs(p.value("bass")+1)<.01f,"state recall");
 p.set("bypass",1);juce::AudioBuffer<float> audio(2,256);audio.clear();audio.setSample(0,0,.2f);p.processBlock(audio,midi);require(audio.getSample(0,0)==.2f,"bypass");
 p.preset(1);std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());require(e->isResizable(),"resizable editor");e->setSize(1400,854);require(e->getWidth()==1400,"editor enlarged");
 auto now=juce::Time::getCurrentTime();juce::Point<float> pt(160,170);juce::MouseEvent doubleClick(juce::Desktop::getInstance().getMainMouseSource(),pt,{},1,0,0,0,0,e.get(),e.get(),now,pt,now,2,false);e->mouseDoubleClick(doubleClick);require(e->getWidth()==800&&e->getHeight()==488,"double click logo restores default size");
 // Dispatch real UI events: constructing the old preset menu reset parameters
 // asynchronously, so inspecting values immediately missed the regression.
 e.reset();p.set("delay",3.21f);p.set("reverb",-2.34f);p.set("delayMode",2);p.set("reverbMode",1);
 for(int attempt=0;attempt<3;++attempt){e.reset(p.createEditor());CFRunLoopRunInMode(kCFRunLoopDefaultMode,.05,false);require(std::abs(p.value("delay")-3.21f)<.02f&&std::abs(p.value("reverb")+2.34f)<.02f&&p.value("delayMode")==2&&p.value("reverbMode")==1,"UI reopen preserves FX");e.reset();}
 e.reset(p.createEditor());e->setSize(1200,732);e.reset();e.reset(p.createEditor());require(e->getWidth()==1200,"editor remembers chosen size");e->setSize(800,488);
 juce::MemoryBlock saved;p.getStateInformation(saved);NFVocal restored;restored.setStateInformation(saved.getData(),int(saved.getSize()));require(std::abs(restored.value("delay")-3.21f)<.02f&&std::abs(restored.value("reverb")+2.34f)<.02f&&restored.value("delayMode")==2&&restored.value("reverbMode")==1,"FX survive fresh processor state restore");
 for(auto* child:e->getChildren())if(auto* combo=dynamic_cast<juce::ComboBox*>(child))if(combo->getNumItems()==6){for(int index=0;index<6;++index){auto label=combo->getItemText(index);require(label.contains(" - "),"preset uses portable ASCII separator");for(auto character:label)require(character>=32&&character<=126,"preset text has no corrupted encoding");}combo->setSelectedId(4,juce::sendNotificationSync);require(std::abs(p.value("delay")+4)<.02f&&std::abs(p.value("reverb")+1)<.02f,"explicit factory preset selection still works");combo->setSelectedId(0,juce::sendNotificationSync);require(std::abs(p.value("delay")+4)<.02f,"empty preset does not reset audio");}
 for(auto* child:e->getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(child)){auto layout=slider->getLookAndFeel().getSliderLayout(*slider);require(layout.sliderBounds.getY()-23>=0&&layout.sliderBounds.getBottom()+23<layout.textBoxBounds.getY(),"fader ends stay inside track and clear numeric value");}
 if(argc>1){auto image=e->createComponentSnapshot(e->getLocalBounds(),true,2.f);juce::File file(argv[1]);file.deleteFile();juce::FileOutputStream stream(file);juce::PNGImageFormat png;require(png.writeImageToStream(image,stream),"screenshot");stream.flush();
 for(int end=0;end<2;++end){for(auto id:{"input","bass","treble","compress","reverb","delay","pitch","output"})p.set(id,(juce::String(id)=="input"||juce::String(id)=="output")?(end?24.f:-24.f):(end?10.f:-10.f));CFRunLoopRunInMode(kCFRunLoopDefaultMode,.03,false);auto extreme=e->createComponentSnapshot(e->getLocalBounds(),true,2.f);auto path=juce::File(argv[1]).getSiblingFile(end?"NF-Vocal-1.0.1-Max.png":"NF-Vocal-1.0.1-Min.png");path.deleteFile();juce::FileOutputStream file(path);require(png.writeImageToStream(extreme,file),"fader extremes screenshot");}
}
 require(energy>0,"nonzero audio");std::cout<<"PASS: 44.1/48/96 kHz, mono/stereo, six presets, all effect modes, impulse/tails, finite audio, state recall, bypass, resize, logo double click. Energy="<<energy<<"\n";
}
