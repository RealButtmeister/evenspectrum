#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// Fixed-storage, stereo-linked weighted overlap-add spectral equalizer.
class SpectralEngine {
public:
    static constexpr int size = 2048, hop = 512, bands = 40;
    struct Settings {
        float amount = 1, responseMs = 250, boostDb = 12, cutDb = 18;
        float lowHz = 35, highHz = 16000, floorDb = -75, outputDb = -6;
        bool rolloff = true;
    } settings;
    std::array<float, bands> inputDb{}, correctionDb{};
    void prepare(double sampleRate) {
        rate = sampleRate; position = elapsed = 0;
        for (auto& x : input) x.fill(0);
        for (auto& x : output) x.fill(0);
        power.fill(0); correctionDb.fill(0); inputDb.fill(-120);
        for (int i=0;i<size;++i) window[i] = std::sqrt(0.5f-0.5f*std::cos(2*pi*i/size));
        top = std::min(20000.0f, float(rate*0.49));
        for (int k=0;k<=size/2;++k) {
            float f = float(k*rate/size);
            coordinates[k] = std::clamp(std::log(std::max(20.0f,f)/20)/std::log(top/20)*(bands-1),0.0f,float(bands-1));
        }
    }
    std::array<float,2> tick(float left, float right, bool bypass = false) {
        std::array<float,2> result{};
        float trim = std::pow(10.0f, settings.outputDb/20);
        const float samples[2] = {left,right};
        for (int c=0;c<2;++c) {
            result[c] = bypass ? input[c][position] : output[c][position]*trim;
            output[c][position] = 0;
            input[c][position] = std::isfinite(samples[c]) ? samples[c] : 0;
        }
        position = (position+1)%size;
        if (++elapsed == hop) { elapsed=0; frame(); }
        return result;
    }
    float frequency(int band) const { return 20*std::pow(top/20,float(band)/(bands-1)); }
private:
    static constexpr float pi = 3.14159265358979323846f;
    double rate=48000;
    float top=20000;
    int position=0, elapsed=0;
    std::array<std::array<float,size>,2> input{},output{};
    std::array<std::array<std::complex<float>,size>,2> spectrum{};
    std::array<float,size> window{};
    std::array<float,size/2+1> coordinates{};
    std::array<float,bands> power{};
    static void fft(std::array<std::complex<float>,size>& a, bool inverse) {
        for (int i=1,j=0;i<size;++i) {
            int bit=size>>1;
            for (;j&bit;bit>>=1) j^=bit;
            j^=bit; if(i<j) std::swap(a[i],a[j]);
        }
        for(int len=2;len<=size;len<<=1) {
            float angle=(inverse?2:-2)*pi/len;
            const std::complex<float> step(std::cos(angle),std::sin(angle));
            for(int i=0;i<size;i+=len) {
                std::complex<float> w(1,0);
                for(int j=0;j<len/2;++j) {
                    auto u=a[i+j], v=a[i+j+len/2]*w;
                    a[i+j]=u+v; a[i+j+len/2]=u-v; w*=step;
                }
            }
        }
        if(inverse) for(auto& v:a) v/=float(size);
    }
    void frame() {
        float rms=0;
        for(int c=0;c<2;++c) {
            for(int i=0;i<size;++i) {
                float s=input[c][(position+i)%size];
                rms+=s*s/(2*size);
                spectrum[c][i]={s*window[i],0};
            }
            fft(spectrum[c],false);
        }
        std::array<float,bands> sums{}, weights{};
        for(int k=1;k<size/2;++k) {
            int b=std::min(bands-2,int(coordinates[k]));
            float mix=coordinates[k]-b;
            float p=(std::norm(spectrum[0][k])+std::norm(spectrum[1][k]))/(float(size)*size);
            sums[b]+=p*(1-mix); weights[b]+=1-mix;
            sums[b+1]+=p*mix; weights[b+1]+=mix;
        }
        const float a=std::exp(-float(hop)/(float(rate)*std::max(0.02f,settings.responseMs*0.001f)));
        float mean=0; int count=0;
        for(int b=0;b<bands;++b) {
            float p=weights[b]>0?sums[b]/weights[b]:0;
            power[b]=a*power[b]+(1-a)*p;
            inputDb[b]=10*std::log10(std::max(1.e-12f,power[b]));
            if(weights[b]>0 && frequency(b)>=settings.lowHz && frequency(b)<=settings.highHz) { mean+=inputDb[b]; ++count; }
        }
        mean=count?mean/count:-120;
        bool active=10*std::log10(std::max(1.e-12f,rms))>settings.floorDb;
        for(int b=0;b<bands;++b) {
            float desired=0;
            if(active && weights[b]>0 && frequency(b)>=settings.lowHz && frequency(b)<=settings.highHz) {
                desired=std::clamp(mean-inputDb[b],-settings.cutDb,settings.boostDb)*settings.amount;
                // Avoid lifting empty bands or the far background below the average spectrum.
                if(desired>0) desired*=std::clamp((inputDb[b]-std::max(-110.0f,mean-45))/15,0.0f,1.0f);
            }
            correctionDb[b]=a*correctionDb[b]+(1-a)*desired;
        }
        for(int k=0;k<=size/2;++k) {
            int b=std::min(bands-2,int(coordinates[k]));
            float mix=coordinates[k]-b;
            float db=correctionDb[b]*(1-mix)+correctionDb[b+1]*mix;
            float gain=std::pow(10.0f,db/20);
            if(settings.rolloff) {
                float f=std::max(0.001f,float(k*rate/size));
                float low=settings.lowHz/f, high=f/settings.highHz;
                gain/=std::sqrt((1+low*low*low*low)*(1+high*high*high*high));
            }
            for(int c=0;c<2;++c) {
                spectrum[c][k]*=gain;
                if(k>0 && k<size/2) spectrum[c][size-k]*=gain;
            }
        }
        for(int c=0;c<2;++c) {
            fft(spectrum[c],true);
            for(int i=0;i<size;++i) output[c][(position+i)%size]+=spectrum[c][i].real()*window[i]*0.5f;
        }
    }
};
