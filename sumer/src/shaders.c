// The shaders of gfx.c, without their first line: gfx.c puts "#version 330" in front of each for the
// desktop and "#version 300 es" (WebGL 2) in the browser. Both take the same GLSL below.
//
// Materials come in each vertex's first texture coordinate (MAT_* in game.h): 0 a thing, 1 ground,
// 2 water, 3 something glowing, 4 people and animals, 5 something built, 6 unlit (rain, trails).

const char *SH_WORLD_VS =
    "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;\n"
    "uniform mat4 mvp;\n"
    "out vec3 wpos; out vec4 vcol; out vec3 vnrm; flat out int vmat;\n"
    "void main(){ wpos=vertexPosition; vcol=vertexColor; vnrm=vertexNormal; vmat=int(vertexTexCoord.x+0.5);\n"
    "  gl_Position=mvp*vec4(vertexPosition,1.0); }\n";

// The sky's color in a direction, the tone curve and the noise: shared by the sky and the world.
const char *SH_COMMON =
    "uniform vec3 camPos; uniform vec3 sunDir; uniform vec3 sunGlow; uniform vec3 skyZen; uniform vec3 skyHor;\n"
    "uniform float exposure; uniform int quality;\n"
    "vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }\n"
    "vec3 toScreen(vec3 c){ return pow(aces(c*exposure),vec3(1.0/2.2)); }\n"
    "vec3 skyColor(vec3 d){ float h=max(d.y,0.0); vec3 c=mix(skyHor,skyZen,pow(h,0.45));\n"
    "  float s=max(dot(d,sunDir),0.0); c+=sunGlow*(pow(s,6.0)*0.10+pow(s,48.0)*0.25);\n"
    "  if(d.y<0.0) c=mix(skyHor,skyHor*0.7,clamp(-d.y*4.0,0.0,1.0)); return c; }\n"
    "float hash(vec3 p){ p=fract(p*0.3183099+0.1); p*=17.0; return fract(p.x*p.y*p.z*(p.x+p.y+p.z)); }\n"
    "float noise(vec3 x){ vec3 i=floor(x); vec3 f=fract(x); f=f*f*(3.0-2.0*f);\n"
    "  return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),\n"
    "             mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1,1,1)),f.x),f.y),f.z); }\n"
    "float fbm(vec3 p){ float a=0.5,s=0.0; int n=quality>0?4:2; for(int i=0;i<4;i++){ if(i>=n) break; s+=a*noise(p); p*=2.03; a*=0.5; } return s; }\n";

const char *SH_SKY_VS =
    "in vec3 vertexPosition; uniform mat4 mvp; out vec3 dir;\n"
    "void main(){ dir=vertexPosition; gl_Position=mvp*vec4(vertexPosition,1.0); }\n";

// The sky dome: the sun's disc, by night the moon and stars, and on the better setting drifting clouds.
const char *SH_SKY_FS =
    "in vec3 dir; out vec4 finalColor;\n"
    "uniform vec3 moonDir; uniform float night; uniform float time; uniform float cloudy;\n"
    "void main(){ vec3 d=normalize(dir); vec3 c=skyColor(d);\n"
    "  c+=sunGlow*6.0*smoothstep(0.9994,0.9998,dot(d,sunDir));\n"
    "  if(night>0.0){ float m=dot(d,moonDir);\n"
    "    c+=vec3(0.75,0.78,0.9)*(smoothstep(0.99935,0.99965,m)*1.4+pow(max(m,0.0),300.0)*0.05)*night;\n"
    "    if(d.y>0.0){ vec3 p=d*360.0; float h=hash(floor(p));\n"
    "      if(h>0.9965){ float k=smoothstep(0.32,0.0,length(fract(p)-0.5)); float tw=0.65+0.35*sin(time*2.3+h*400.0);\n"
    "        c+=vec3(0.85,0.88,1.0)*k*tw*night*smoothstep(0.0,0.15,d.y)*(h-0.9965)*100.0; } } }\n"
    "  if(quality>0 && d.y>0.0){ vec2 uv=d.xz/(d.y+0.12);\n"
    "    float n=fbm(vec3(uv*1.2+vec2(time*0.006,time*0.002),time*0.01));\n"
    "    float cover=smoothstep(0.52-cloudy*0.35,0.82-cloudy*0.2,n);\n"
    "    vec3 cc=mix(skyHor*1.15+sunGlow*0.18,vec3(0.42,0.44,0.47)*(skyHor.g*2.0+0.05),cloudy);\n"
    "    c=mix(c,cc,cover*0.7*smoothstep(0.0,0.25,d.y)); }\n"
    "  finalColor=vec4(toScreen(c),1.0); }\n";

// Everything in the world. Normals come with the vertices where the draw code gives them (round
// things, the ground) and from the screen-space slope of the surface where it does not.
const char *SH_WORLD_FS =
    "in vec3 wpos; in vec4 vcol; in vec3 vnrm; flat in int vmat; out vec4 finalColor;\n"
    "uniform vec3 lightDir; uniform vec3 lightCol; uniform vec3 ambSky; uniform vec3 ambGround; uniform vec3 torchCol;\n"
    "uniform float fogDensity; uniform float time; uniform float worldHalf; uniform float night; uniform float wet;\n"
    "uniform float shadowTexel; uniform float shadowRange; uniform float groundTile; uniform float groundN;\n"
    "uniform float heightScale; uniform float heightBase;\n"
    "uniform int torchSlots; uniform int gridN; uniform float gridCell; uniform float torchRadius;\n"
    "uniform int dynCount; uniform vec4 dynLights[8];\n"
    "uniform mat4 lightVP; uniform sampler2D shadowMap; uniform sampler2D heightMap; uniform sampler2D torchGrid; uniform sampler2D torchData;\n"
    "float unpackDepth(vec4 c){ return dot(c.rgb,vec3(1.0,1.0/255.0,1.0/65025.0)); }\n"
    "float tap(vec2 uv,float z){ return z>unpackDepth(texture(shadowMap,uv))?0.0:1.0; }\n"
    "float bilinearTap(vec2 uv,float z){ vec2 st=uv/shadowTexel-0.5; vec2 f=fract(st); vec2 b=(floor(st)+0.5)*shadowTexel;\n"
    "  return mix(mix(tap(b,z),tap(b+vec2(shadowTexel,0.0),z),f.x),mix(tap(b+vec2(0.0,shadowTexel),z),tap(b+vec2(shadowTexel),z),f.x),f.y); }\n"
    "float shadowAt(vec3 p,vec3 n,float ndl){\n"
    "  vec4 lp=lightVP*vec4(p+n*(0.04+0.10*(1.0-ndl)),1.0); vec3 sc=lp.xyz/lp.w*0.5+0.5;\n"
    "  vec2 e=min(sc.xy,1.0-sc.xy); float fade=smoothstep(0.0,0.08,min(e.x,e.y));\n"
    "  if(fade<=0.0||sc.z>=1.0) return 1.0;\n"
    "  float z=sc.z-(0.03+0.08*(1.0-ndl))/shadowRange; float s;\n"
    "  if(quality==0) s=bilinearTap(sc.xy,z);\n"
    "  else s=0.25*(bilinearTap(sc.xy+vec2(-0.8,-0.8)*shadowTexel,z)+bilinearTap(sc.xy+vec2(0.8,-0.8)*shadowTexel,z)\n"
    "             +bilinearTap(sc.xy+vec2(-0.8,0.8)*shadowTexel,z)+bilinearTap(sc.xy+vec2(0.8,0.8)*shadowTexel,z));\n"
    "  return mix(1.0,s,fade); }\n"
    "float groundAt(vec2 xz){ vec2 uv=((xz+worldHalf)/groundTile+0.5)/groundN; return texture(heightMap,uv).r*heightScale+heightBase; }\n"
    "float falloff(float d){ float k=max(1.0-d/torchRadius,0.0); return k*k; }\n"
    "vec3 torchLight(vec3 p,vec3 n){ float sum=0.0;\n"
    "  ivec2 c=ivec2(floor((p.xz+worldHalf)/gridCell));\n"
    "  if(c.x>=0&&c.y>=0&&c.x<gridN&&c.y<gridN){\n"
    "    for(int s=0;s<8;s++){ if(s>=torchSlots) break;\n"
    "      vec4 g=texelFetch(torchGrid,ivec2(c.x*2+s/4,c.y),0); int idx=int(g[s&3]*255.0+0.5); if(idx==0) break;\n"
    "      vec4 t=texelFetch(torchData,ivec2(idx-1,0),0); vec3 L=t.xyz-p; float d=length(L);\n"
    "      float fl=0.88+0.12*sin(time*9.0+t.w*1.7)*sin(time*5.3+t.w);\n"
    "      sum+=falloff(d)*(max(dot(n,L/max(d,0.001)),0.0)*0.9+0.1)*fl; } }\n"
    "  for(int i=0;i<8;i++){ if(i>=dynCount) break; vec3 L=dynLights[i].xyz-p; float d=length(L);\n"
    "    sum+=falloff(d)*(max(dot(n,L/max(d,0.001)),0.0)*0.9+0.1)*dynLights[i].w; }\n"
    "  return sum*torchCol*night; }\n"
    "void main(){\n"
    "  if(vmat==6){ finalColor=vcol; return; }\n"
    "  vec3 base=pow(vcol.rgb,vec3(2.2));\n"
    "  if(vmat==3){ finalColor=vec4(toScreen(base*2.6),vcol.a); return; }\n"
    "  vec3 toCam=camPos-wpos; float dist=length(toCam); vec3 V=toCam/max(dist,0.001);\n"
    "  vec3 fn=normalize(cross(dFdx(wpos),dFdy(wpos))); if(dot(fn,toCam)<0.0) fn=-fn;\n"
    "  vec3 n=dot(vnrm,vnrm)>0.25?normalize(vnrm):fn;\n"
    // fine detail fades with distance, where it would only shimmer
    "  float near=1.0-smoothstep(30.0,140.0,dist);\n"
    "  float lum=dot(vcol.rgb,vec3(0.3,0.59,0.11)), sat=max(vcol.r,max(vcol.g,vcol.b))-min(vcol.r,min(vcol.g,vcol.b));\n"
    "  float rough=0.85, spec=0.04; bool water=false;\n"
    "  if(vmat==1){\n"   // ground: soil and sand with grain, a gentle bump, darker and glossy in the rain
    "    float big=fbm(wpos*vec3(0.08,0.0,0.08)), fine=noise(wpos*vec3(2.5,0.0,2.5)), grit=hash(floor(wpos*18.0));\n"
    "    base*=0.80+0.30*big+(0.10*fine-0.06*step(0.93,grit))*near;\n"
    "    vec3 q=wpos*vec3(1.7,0.0,1.7);\n"
    "    vec3 g=vec3(noise(q+vec3(0.1,0.0,0.0))-noise(q-vec3(0.1,0.0,0.0)),0.0,noise(q+vec3(0.0,0.0,0.1))-noise(q-vec3(0.0,0.0,0.1)));\n"
    "    n=normalize(n-g*0.6*near); rough=mix(0.97,0.35,wet); base*=mix(1.0,0.72,wet);\n"
    "  } else if(vmat==2){\n"   // water: ripples, the sky in it, the sun glinting, darker where deep
    "    water=true; vec3 p=wpos*0.6; float e=0.08;\n"
    "    float h0=fbm(vec3(p.x+time*0.3,time*0.2,p.z)), hx=fbm(vec3(p.x+e+time*0.3,time*0.2,p.z)), hz=fbm(vec3(p.x+time*0.3,time*0.2,p.z+e));\n"
    "    n=normalize(vec3(-(hx-h0)/e*0.35,1.0,-(hz-h0)/e*0.35)); n=normalize(mix(n,vec3(0.0,1.0,0.0),1.0-near*0.8));\n"
    "    rough=0.05; spec=0.02; float deep=max(wpos.y-groundAt(wpos.xz),0.0);\n"
    "    base=mix(vec3(0.05,0.10,0.08),vec3(0.015,0.05,0.055),smoothstep(0.2,2.0,deep));\n"
    "  } else if(vmat==5 && lum>0.42 && sat<0.32 && vcol.r>=vcol.g && vcol.g>=vcol.b*0.9){\n" // mud brick and plaster
    "    if(abs(fn.y)<0.35){\n"
    "      float u=abs(fn.x)>abs(fn.z)?wpos.z:wpos.x; float bh=0.11, bw=0.36;\n"
    "      float row=floor(wpos.y/bh), uu=u/bw+0.5*mod(row,2.0), col=floor(uu); vec2 f=vec2(fract(uu),fract(wpos.y/bh));\n"
    "      float m=smoothstep(0.0,0.07,f.x)*smoothstep(1.0,0.93,f.x)*smoothstep(0.0,0.14,f.y)*smoothstep(1.0,0.86,f.y);\n"
    "      m=mix(0.86,m,near);\n"
    "      float worn=fbm(wpos*1.3); float tint=hash(vec3(col,row,3.0));\n"
    "      vec3 brick=base*(0.84+0.24*tint*near)*(0.85+0.3*worn);\n"
    "      float plaster=smoothstep(0.45,0.6,fbm(wpos*0.35+vec3(5.0)));\n"   // patches where the plaster still holds
    "      vec3 plast=base*(1.02+0.1*noise(wpos*6.0)*near);\n"
    "      base=mix(mix(base*0.62,brick,m),plast,plaster*0.65);\n"
    "      n=normalize(n+(1.0-m)*(1.0-plaster)*0.25*near*vec3(0.0,(f.y<0.5?-1.0:1.0),0.0));\n"
    "    } else { base*=0.86+0.24*fbm(wpos*0.9); }\n"
    "  } else if(vmat!=4 && vcol.g>vcol.r*1.05 && vcol.g>vcol.b){\n" // leaves: blotches and a sheen
    "    base*=0.75+0.45*fbm(wpos*2.2); rough=0.6;\n"
    "  } else if(vmat!=4 && vcol.r>vcol.g && vcol.g>vcol.b && lum<0.45 && sat>0.12){\n" // wood: grain along the length
    "    base*=0.82+0.3*noise(vec3(wpos.x*9.0,wpos.y*0.7,wpos.z*9.0))*near+0.1*fbm(wpos*1.5); rough=0.75;\n"
    "  } else if(vmat==4){\n"   // people and animals: cloth weave, a soft sheen on skin
    "    base*=0.92+0.12*noise(wpos*40.0)*near; rough=0.7;\n"
    "  } else { base*=0.88+0.22*fbm(wpos*1.6); }\n"
    "  float ndl=max(dot(n,lightDir),0.0);\n"
    "  float sh=ndl>0.0?shadowAt(wpos,n,ndl):0.0;\n"
    "  float above=wpos.y-groundAt(wpos.xz);\n"
    "  float ao=(vmat==1||water)?1.0:mix(0.5,1.0,smoothstep(0.0,1.6,above))*(0.78+0.22*max(n.y,0.0));\n"
    "  vec3 amb=mix(ambGround,ambSky,n.y*0.5+0.5)*ao;\n"
    "  vec3 torch=night>0.0?torchLight(wpos,n):vec3(0.0);\n"
    "  vec3 H=normalize(lightDir+V); float sp=pow(max(dot(n,H),0.0),mix(512.0,8.0,rough))*mix(40.0,0.05,rough)*ndl;\n"
    "  float fres=spec+(1.0-spec)*pow(1.0-max(dot(n,V),0.0),5.0);\n"
    "  vec3 col=base*(lightCol*ndl*sh+amb+torch)+lightCol*sp*sh*fres;\n"
    "  if(water){ vec3 r=reflect(-V,n); r.y=abs(r.y);\n"
    "    col=mix(base*(amb+lightCol*0.1+torch),skyColor(r)*1.1,fres)+min(lightCol*sp*sh*0.35,vec3(2.5)); }\n"
    "  float fog=1.0-exp(-dist*fogDensity);\n"
    "  col=mix(col,skyColor(-V)*1.05,fog);\n"
    "  finalColor=vec4(toScreen(col),vcol.a);\n"
    "}\n";

// The sun's view, for the shadow: depth packed into the color, which every WebGL 2 device can render
// to. Water, glowing and unlit things cast no shadow.
const char *SH_DEPTH_VS =
    "in vec3 vertexPosition; in vec2 vertexTexCoord; uniform mat4 mvp; flat out int vmat;\n"
    "void main(){ vmat=int(vertexTexCoord.x+0.5); gl_Position=mvp*vec4(vertexPosition,1.0); }\n";

const char *SH_DEPTH_FS =
    "flat in int vmat; out vec4 finalColor;\n"
    "void main(){ if(vmat==2||vmat==3||vmat==6) discard;\n"
    "  vec3 e=fract(gl_FragCoord.z*vec3(1.0,255.0,65025.0)); e-=e.yzz*vec3(1.0/255.0,1.0/255.0,0.0); finalColor=vec4(e,1.0); }\n";

// After the scene: the bright parts for the glow, a blur, then FXAA, the glow and a vignette.
const char *SH_BRIGHT_FS =
    "in vec2 fragTexCoord; uniform sampler2D texture0; out vec4 finalColor;\n"
    "void main(){ vec3 c=texture(texture0,fragTexCoord).rgb; float l=dot(c,vec3(0.3,0.59,0.11)); finalColor=vec4(c*smoothstep(0.78,1.0,l),1.0); }\n";

const char *SH_BLUR_FS =
    "in vec2 fragTexCoord; uniform sampler2D texture0; uniform vec2 dir; out vec4 finalColor;\n"
    "void main(){ float w[5]=float[](0.227,0.194,0.121,0.054,0.016); vec3 c=texture(texture0,fragTexCoord).rgb*w[0];\n"
    "  for(int i=1;i<5;i++){ c+=texture(texture0,fragTexCoord+dir*float(i)).rgb*w[i]; c+=texture(texture0,fragTexCoord-dir*float(i)).rgb*w[i]; }\n"
    "  finalColor=vec4(c,1.0); }\n";

const char *SH_POST_FS =
    "in vec2 fragTexCoord; uniform sampler2D texture0; uniform sampler2D bloom; uniform vec2 texel; uniform float bloomAmt; out vec4 finalColor;\n"
    "vec3 fx(vec2 uv){ vec3 rgbM=texture(texture0,uv).rgb; vec3 L=vec3(0.299,0.587,0.114);\n"
    "  float lNW=dot(texture(texture0,uv+vec2(-1.0,-1.0)*texel).rgb,L), lNE=dot(texture(texture0,uv+vec2(1.0,-1.0)*texel).rgb,L);\n"
    "  float lSW=dot(texture(texture0,uv+vec2(-1.0,1.0)*texel).rgb,L), lSE=dot(texture(texture0,uv+vec2(1.0,1.0)*texel).rgb,L), lM=dot(rgbM,L);\n"
    "  float mn=min(lM,min(min(lNW,lNE),min(lSW,lSE))), mx=max(lM,max(max(lNW,lNE),max(lSW,lSE)));\n"
    "  vec2 d=vec2(-((lNW+lNE)-(lSW+lSE)),((lNW+lSW)-(lNE+lSE)));\n"
    "  float red=max((lNW+lNE+lSW+lSE)*0.03125,1.0/128.0), rcp=1.0/(min(abs(d.x),abs(d.y))+red);\n"
    "  d=clamp(d*rcp,-8.0,8.0)*texel;\n"
    "  vec3 a=0.5*(texture(texture0,uv+d*(1.0/3.0-0.5)).rgb+texture(texture0,uv+d*(2.0/3.0-0.5)).rgb);\n"
    "  vec3 b=a*0.5+0.25*(texture(texture0,uv+d*-0.5).rgb+texture(texture0,uv+d*0.5).rgb);\n"
    "  float lb=dot(b,L); return (lb<mn||lb>mx)?a:b; }\n"
    "void main(){ vec2 uv=fragTexCoord; vec3 c=fx(uv); if(bloomAmt>0.0) c+=texture(bloom,uv).rgb*bloomAmt;\n"
    "  float l=dot(c,vec3(0.299,0.587,0.114)); c=mix(vec3(l),c,1.18); c=clamp((c-0.5)*1.06+0.5,0.0,1.0);\n"
    "  vec2 q=uv-0.5; c*=1.0-0.35*dot(q,q)*1.6; finalColor=vec4(c,1.0); }\n";
