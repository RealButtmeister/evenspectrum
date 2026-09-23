#include "../Source/SpectralEngine.h"
#include <iostream>
#include <random>
#include <vector>
#include <stdexcept>
static void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    try {
        for(double rate:{44100.,48000.,96000.}) {
            SpectralEngine e; e.prepare(rate); e.settings.amount=0; e.settings.rolloff=false; e.settings.outputDb=0;
            std::mt19937 random(42); std::uniform_real_distribution<float> noise(-0.3f,0.3f);
            std::vector<float> data(32000); for(auto& x:data) x=noise(random);
            float error=0;
            for(int i=0;i<int(data.size());++i) {
                auto y=e.tick(data[i],-data[i]);
                if(i>=e.size) error=std::max(error,std::abs(y[0]-data[i-e.size]));
                require(std::abs(y[0]+y[1])<1.e-5f,"stereo linking changes polarity relationship");
            }
            require(error<2.e-5f,"unity reconstruction or declared latency incorrect");
            e.prepare(rate);
            for(int i=0;i<16000;++i) {
                auto y=e.tick(0,0); require(y[0]==0 && y[1]==0,"silence generates audio");
            }
            for(int i=0;i<12000;++i) {
                auto y=e.tick(data[i],data[i],true);
                require(std::abs(y[0]-(i>=e.size?data[i-e.size]:0))<1.e-7f,"bypass latency incorrect");
            }
        }
        SpectralEngine e; e.prepare(48000); e.settings.rolloff=false; e.settings.outputDb=0;
        std::mt19937 random(13); std::normal_distribution<float> noise(0,0.02f);
        double inCos=0,outCos=0,inEnergy=0,outEnergy=0;
        for(int i=0;i<480000;++i) {
            float input=0.2f*std::sin(2*3.141592653589793*1500*i/48000)+noise(random);
            auto y=e.tick(input,input);
            require(std::isfinite(y[0]),"nonfinite output");
            if(i>240000) {
                inCos+=input*std::sin(2*3.141592653589793*1500*i/48000);
                outCos+=y[0]*std::sin(2*3.141592653589793*1500*(i-e.size)/48000);
                inEnergy+=input*input; outEnergy+=y[0]*y[0];
            }
        }
        require(std::abs(outCos/inCos)<0.7,"prominent tone was not reduced");
        bool lifted=false;
        for(float db:e.correctionDb) { require(db>=-18.001f && db<=12.001f,"correction exceeds limits"); if(db>1) lifted=true; }
        require(lifted,"quieter spectrum not lifted");
        auto toneGain=[](float frequency) {
            SpectralEngine filter; filter.prepare(48000);
            filter.settings.amount=0; filter.settings.outputDb=0;
            filter.settings.lowHz=200; filter.settings.highHz=8000;
            double energy=0; int count=0;
            for(int i=0;i<96000;++i) {
                float x=0.1f*std::sin(2*3.141592653589793*frequency*i/48000);
                auto y=filter.tick(x,x);
                if(i>48000) {energy+=y[0]*y[0]; ++count;}
            }
            return std::sqrt(energy/count)/std::sqrt(0.005);
        };
        require(toneGain(50)<0.15,"low rolloff ineffective");
        require(toneGain(16000)<0.3,"high rolloff ineffective");
        require(toneGain(1000)>0.97,"rolloff damages midrange");
        std::cout<<"PASS: reconstruction, latency, 3 sample rates, silence, stereo, bypass, finite output, tone reduction, quiet-band lift, correction limits, low/high rolloffs, midrange preservation\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
