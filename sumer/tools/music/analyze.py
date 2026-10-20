import numpy as np, sys
names=['day','fight','night']
for m in range(3):
    x=np.fromfile(f'm{m}.raw',dtype=np.float32).reshape(-1,2)
    mono=x.mean(1)
    peak=np.abs(x).max(); rms=np.sqrt((x**2).mean())
    nan=np.isnan(x).sum()
    win=44100*5
    seg=[np.sqrt((mono[i:i+win]**2).mean()) for i in range(0,len(mono)-win+1,win)]
    print(f"{names[m]:6s} peak {peak:.3f} ({20*np.log10(peak):.1f} dBFS) rms {20*np.log10(rms):.1f} dBFS nan {nan} clip>0.98: {(np.abs(x)>0.98).sum()}")
    print("   5s rms dB:", ' '.join(f"{20*np.log10(s+1e-9):.0f}" for s in seg))
    # stereo balance
    print("   L/R rms dB: %.1f / %.1f   DC %.5f" % (20*np.log10(np.sqrt((x[:,0]**2).mean())), 20*np.log10(np.sqrt((x[:,1]**2).mean())), mono.mean()))
    # spectral centroid
    sp=np.abs(np.fft.rfft(mono[:44100*30]*np.hanning(44100*30)))
    fr=np.fft.rfftfreq(44100*30,1/44100)
    print("   centroid %.0f Hz, 90%% energy below %.0f Hz" % ((sp*fr).sum()/sp.sum(), fr[np.searchsorted(np.cumsum(sp**2)/np.sum(sp**2),0.9)]))
