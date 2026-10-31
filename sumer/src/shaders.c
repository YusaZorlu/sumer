// The shaders of gfx.c, without their first line: gfx.c puts "#version 330" in front of each for the
// desktop and "#version 300 es" (WebGL 2) in the browser. Both take the same GLSL below.
//
// Materials come in each vertex's first texture coordinate (MAT_* in game.h): 0 a thing, 1 ground,
// 2 water, 3 something glowing, 4 people and animals, 5 something built, 6 unlit (rain, trails). For people
// the other three hold where on the body the point is (1/64 m, x and z from -2 m), and the color's alpha
// says what it is (255 skin and the rest, 254 woven cloth, 253 fleece or fur, 252 leather, 251 metal). On
// the ground, alpha 254 marks a road.

const char *SH_WORLD_VS =
    "in vec3 vertexPosition; in vec4 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;\n"
    "uniform mat4 mvp;\n"
    "out vec3 wpos; out vec4 vcol; out vec3 vnrm; out vec3 vbody; flat out int vmat;\n"
    "void main(){ wpos=vertexPosition; vcol=vertexColor; vnrm=vertexNormal; vmat=int(vertexTexCoord.x+0.5);\n"
    "  vbody=vertexTexCoord.yzw/64.0;\n"
    "  gl_Position=mvp*vec4(vertexPosition,1.0); }\n";

// The sky's color in a direction, the tone curve and the noise: shared by the sky and the world.
// The sky: a plain gradient from the horizon up with the sun's glow, and over it the photographed skies
// (skyDay, skyDusk, skyGrey: the upper half of a panorama each, the zenith at the top), blended by skyMix
// (w: how much they show) and turned round by skyTurn. skyFlat is the plain one, for the haze on the land.
const char *SH_COMMON =
    "uniform vec3 camPos; uniform vec3 sunDir; uniform vec3 sunGlow; uniform vec3 skyZen; uniform vec3 skyHor;\n"
    "uniform float exposure; uniform int quality;\n"
    "uniform sampler2D skyDay; uniform sampler2D skyDusk; uniform sampler2D skyGrey; uniform vec4 skyMix; uniform vec3 skyTurn; uniform float skyLevel;\n"
    "vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }\n"
    "vec3 toScreen(vec3 c){ return pow(aces(c*exposure),vec3(1.0/2.2)); }\n"
    "vec3 sunHalo(vec3 d){ float s=max(dot(d,sunDir),0.0); return sunGlow*(pow(s,6.0)*0.10+pow(s,48.0)*0.25); }\n"
    "vec3 skyFlat(vec3 d){ if(d.y<0.0) return mix(skyHor,skyHor*0.7,clamp(-d.y*4.0,0.0,1.0));\n"
    "  return mix(skyHor,skyZen,pow(d.y,0.45))+sunHalo(d); }\n"
    "vec2 skyUV(vec3 d){ return vec2(atan(d.z,d.x)*0.1591549+0.5,clamp(acos(clamp(d.y,0.0,1.0))*0.6366198,0.002,0.998)); }\n"
    "vec3 skyPicture(vec3 d){ vec2 uv=skyUV(d); vec3 c=vec3(0.0);\n"
    "  if(skyMix.x>0.001) c+=pow(texture(skyDay,uv+vec2(skyTurn.x,0.0)).rgb,vec3(2.2))*skyMix.x;\n"
    "  if(skyMix.y>0.001) c+=pow(texture(skyDusk,uv+vec2(skyTurn.y,0.0)).rgb,vec3(2.2))*skyMix.y;\n"
    "  if(skyMix.z>0.001) c+=pow(texture(skyGrey,uv+vec2(skyTurn.z,0.0)).rgb,vec3(2.2))*skyMix.z;\n"
    "  return c*skyLevel; }\n"
    "vec3 skyColor(vec3 d){ if(d.y<0.0||skyMix.w<=0.0) return skyFlat(d);\n"
    "  return mix(mix(skyHor,skyZen,pow(d.y,0.45)),skyPicture(d),skyMix.w*smoothstep(0.0,0.04,d.y))+sunHalo(d); }\n"
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
    "    if(d.y>0.0){ vec3 p=d*360.0; float h=hash(floor(p)); float clear=1.0;\n"   // stars hide behind the clouds
    "      if(skyMix.w>0.0){ vec3 q=texture(skyDay,skyUV(d)+vec2(skyTurn.x,0.0)).rgb; clear=(1.0-smoothstep(0.62,0.9,q.r/max(q.b,0.02)))*(1.0-skyMix.z); }\n"
    "      if(h>0.9965){ float k=smoothstep(0.32,0.0,length(fract(p)-0.5)); float tw=0.65+0.35*sin(time*2.3+h*400.0);\n"
    "        c+=vec3(0.85,0.88,1.0)*k*tw*night*clear*smoothstep(0.0,0.15,d.y)*(h-0.9965)*100.0; } } }\n"
    "  if(quality>0 && d.y>0.0 && skyMix.w<=0.0){ vec2 uv=d.xz/(d.y+0.12);\n"
    "    float n=fbm(vec3(uv*1.2+vec2(time*0.006,time*0.002),time*0.01));\n"
    "    float cover=smoothstep(0.52-cloudy*0.35,0.82-cloudy*0.2,n);\n"
    "    vec3 cc=mix(skyHor*1.15+sunGlow*0.18,vec3(0.42,0.44,0.47)*(skyHor.g*2.0+0.05),cloudy);\n"
    "    c=mix(c,cc,cover*0.7*smoothstep(0.0,0.25,d.y)); }\n"
    "  finalColor=vec4(toScreen(c),1.0); }\n";

// Everything in the world. Normals come with the vertices where the draw code gives them (round
// things, the ground) and from the screen-space slope of the surface where it does not.
const char *SH_WORLD_FS =
    "in vec3 wpos; in vec4 vcol; in vec3 vnrm; in vec3 vbody; flat in int vmat; out vec4 finalColor;\n"
    "uniform vec3 lightDir; uniform vec3 lightCol; uniform vec3 ambSky; uniform vec3 ambGround; uniform vec3 torchCol;\n"
    "uniform float fogDensity; uniform float time; uniform float worldHalf; uniform float night; uniform float wet;\n"
    "uniform float shadowTexel; uniform float shadowRange; uniform float groundTile; uniform float groundN;\n"
    "uniform float heightScale; uniform float heightBase;\n"
    "uniform int torchSlots; uniform int gridN; uniform float gridCell; uniform float torchRadius;\n"
    "uniform int dynCount; uniform vec4 dynLights[8];\n"
    "uniform mat4 lightVP; uniform sampler2D shadowMap; uniform sampler2D heightMap; uniform sampler2D torchGrid; uniform sampler2D torchData;\n"
    "uniform sampler2D detailMap; uniform sampler2D detailMap2; uniform sampler2D detailMap3;\n"
    // A photo's light and dark, about -1..1: on the faces turned most toward each axis, blended (no UVs needed)
    "float det(vec3 p,vec3 n,float s,int c){ vec3 w=pow(abs(n),vec3(4.0)); w/=w.x+w.y+w.z;\n"
    "  return (texture(detailMap,p.zy*s)[c]*w.x+texture(detailMap,p.xz*s)[c]*w.y+texture(detailMap,p.xy*s)[c]*w.z)*3.2-1.6; }\n"
    "float det3(vec3 p,vec3 n,float s,int c){ vec3 w=pow(abs(n),vec3(4.0)); w/=w.x+w.y+w.z;\n"
    "  return (texture(detailMap3,p.zy*s)[c]*w.x+texture(detailMap3,p.xz*s)[c]*w.y+texture(detailMap3,p.xy*s)[c]*w.z)*3.2-1.6; }\n"
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
    "    float big=fbm(wpos*vec3(0.08,0.0,0.08)); vec2 uv=wpos.xz*0.31, e=vec2(0.004,0.0);\n"
    "    float d=texture(detailMap,uv).r, dx=texture(detailMap,uv+e).r-d, dz=texture(detailMap,uv+e.yx).r-d;\n"
    "    float far=texture(detailMap,wpos.xz*0.043+0.37).r-0.5;\n"   // a second, larger scale breaks the repeat
    "    base*=0.80+0.30*big+(d-0.5)*0.85*mix(0.45,1.0,near)+far*0.25;\n"
    "    n=normalize(n-vec3(dx,0.0,dz)*6.0*near); rough=mix(0.97,0.35,wet); base*=mix(1.0,0.72,wet);\n"
    "    if(vcol.a<0.999){ float r=texture(detailMap2,wpos.xz*0.22).a; base*=1.0+(r-0.5)*0.7*mix(0.4,1.0,near); }\n"
    "  } else if(vmat==2){\n"   // water: ripples, the sky in it, the sun glinting, darker where deep
    "    water=true; vec3 p=wpos*0.6; float e=0.08;\n"
    "    float h0=fbm(vec3(p.x+time*0.3,time*0.2,p.z)), hx=fbm(vec3(p.x+e+time*0.3,time*0.2,p.z)), hz=fbm(vec3(p.x+time*0.3,time*0.2,p.z+e));\n"
    "    n=normalize(vec3(-(hx-h0)/e*0.35,1.0,-(hz-h0)/e*0.35)); n=normalize(mix(n,vec3(0.0,1.0,0.0),1.0-near*0.8));\n"
    "    rough=0.05; spec=0.02; float deep=max(wpos.y-groundAt(wpos.xz),0.0);\n"
    "    base=mix(vec3(0.05,0.10,0.08),vec3(0.015,0.05,0.055),smoothstep(0.2,2.0,deep));\n"
    "  } else if(vmat!=4 && vcol.a>0.982 && vcol.a<0.998){\n" // trees and fields: bark, leaves, tilled earth, straw
    "    int tag=int(255.0-vcol.a*255.0+0.5); float s=tag==1?0.9:tag==2?0.9:tag==3?0.3:1.3;\n"
    "    base*=max(1.0+det3(wpos,n,s,tag-1)*(tag==2?0.7:tag==1?0.6:0.5)*mix(0.4,1.0,near),0.2);\n"
    "    if(tag==2){ base*=0.78+0.42*fbm(wpos*2.2); rough=0.95; } else rough=tag==4?0.7:0.92;\n"
    "  } else if(vmat==5 && lum>0.42 && sat<0.32 && vcol.r>=vcol.g && vcol.g>=vcol.b*0.9){\n" // mud brick and plaster
    "    if(abs(fn.y)<0.35){\n"
    "      float u=abs(fn.x)>abs(fn.z)?wpos.z:wpos.x; vec2 uv=vec2(u,wpos.y)*0.55;\n"
    "      float b=texture(detailMap,uv).g, by=texture(detailMap,uv+vec2(0.0,0.003)).g-b;\n"
    "      float worn=fbm(wpos*1.3);\n"
    "      float plaster=smoothstep(0.45,0.6,fbm(wpos*0.35+vec3(5.0)));\n"   // patches where the plaster still holds
    "      vec3 brick=base*(1.0+(b-0.5)*0.9*mix(0.4,1.0,near))*(0.85+0.3*worn);\n"
    "      vec3 plast=base*(1.02+(b-0.5)*0.15+0.1*noise(wpos*6.0)*near);\n"
    "      base=mix(brick,plast,plaster*0.65);\n"
    "      n=normalize(n+vec3(0.0,-by*6.0,0.0)*(1.0-plaster)*near);\n"
    "    } else { base*=0.86+0.2*fbm(wpos*0.9)+0.25*det(wpos,fn,0.31,0)*0.25*near; }\n"
    "  } else if(vmat!=4 && vcol.g>vcol.r*1.05 && vcol.g>vcol.b){\n" // leaves: blotches and a sheen
    "    base*=0.75+0.45*fbm(wpos*2.2); rough=0.6;\n"
    "  } else if(vmat!=4 && vcol.r>vcol.g && vcol.g>vcol.b && lum<0.45 && sat>0.12){\n" // wood: grain along the length
    "    base*=0.9+0.1*fbm(wpos*1.5)+det(wpos,n,0.45,2)*0.22*mix(0.4,1.0,near); rough=0.75;\n"
    "  } else if(vmat==4){\n"   // people and animals: the photo grain of what they wear, a soft sheen on skin
    "    int tag=int(255.0-vcol.a*255.0+0.5);\n"
    "    if(tag>=1&&tag<=3){ vec3 ln=cross(dFdx(vbody),dFdy(vbody)); ln=dot(ln,ln)>1e-12?normalize(ln):vec3(0.0,0.0,1.0);\n"
    "      vec3 w=pow(abs(ln),vec3(4.0)); w/=w.x+w.y+w.z; float s=tag==1?3.0:tag==2?2.0:3.0; int c=tag-1;\n"
    "      float d=texture(detailMap2,vbody.zy*s)[c]*w.x+texture(detailMap2,vbody.xz*s)[c]*w.y+texture(detailMap2,vbody.xy*s)[c]*w.z;\n"
    "      base*=1.0+(d-0.5)*(tag==2?1.1:0.8)*mix(0.4,1.0,near); rough=tag==3?0.55:0.92;\n"
    "    } else if(tag==4){ base*=0.9+0.2*noise(wpos*30.0)*near; rough=0.28; spec=0.35;\n"
    "    } else { base*=0.92+0.12*noise(wpos*40.0)*near; rough=0.7; }\n"
    "  } else if(sat<0.12){ base*=0.9+0.12*fbm(wpos*1.6)+det(wpos,n,0.35,3)*0.2*mix(0.4,1.0,near);\n"   // stone, rock
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
    "    col=mix(base*(amb+lightCol*0.1+torch),skyColor(r)*1.1,fres)+min(lightCol*sp*sh*0.35,vec3(2.5));\n"
    // the sun glittering on the ripples: tiny bright points where the sun's reflection falls, never still
    "    float glint=pow(max(dot(r,sunDir),0.0),90.0)*smoothstep(0.78,0.98,noise(vec3(wpos.x*7.0,time*2.1,wpos.z*7.0)));\n"
    "    col+=sunGlow*glint*sh*near*4.0; }\n"
    "  float fog=1.0-exp(-dist*fogDensity*(0.55+0.75*exp(-max(wpos.y,0.0)*0.05)));\n"   // thicker down by the ground
    "  vec3 haze=skyFlat(-V); float hl=dot(haze,vec3(0.3,0.59,0.11));\n"
    "  col=mix(col,mix(haze,vec3(0.78,0.66,0.52)*hl*1.15,0.22)*1.05,fog);\n"
    "  finalColor=vec4(toScreen(col),(vmat==4||vmat==1||vcol.a>0.982)?1.0:vcol.a);\n"
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
    "  float l=dot(c,vec3(0.299,0.587,0.114)); c=mix(vec3(l),c,1.12); c=clamp((c-0.5)*1.06+0.5,0.0,1.0);\n"
    // warm and a little dusty, like a photograph of the dry land: reds up and blues down in the middle tones,
    // the shadows lifted toward brown
    "  c=pow(c,vec3(0.96,1.0,1.07)); c+=vec3(0.018,0.010,0.0)*(1.0-c);\n"
    "  vec2 q=uv-0.5; c*=1.0-0.35*dot(q,q)*1.6; finalColor=vec4(c,1.0); }\n";
