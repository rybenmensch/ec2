/*******************************************************************************************************************
Cycling '74 License for Max-Generated Code for Export
Copyright (c) 2016 Cycling '74
The code that Max generates automatically and that end users are capable of exporting and using, and any
  associated documentation files (the “Software”) is a work of authorship for which Cycling '74 is the author
  and owner for copyright purposes.  A license is hereby granted, free of charge, to any person obtaining a
  copy of the Software (“Licensee”) to use, copy, modify, merge, publish, and distribute copies of the Software,
  and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The Software is licensed to Licensee only for non-commercial use. Users who wish to make commercial use of the
  Software must contact the copyright owner to determine if a license for commercial use is available, and the
  terms and conditions for same, which may include fees or royalties. For commercial use, please send inquiries
  to licensing (at) cycling74.com.  The determination of whether a use is commercial use or non-commercial use is based
  upon the use, not the user. The Software may be used by individuals, institutions, governments, corporations, or
  other business whether for-profit or non-profit so long as the use itself is not a commercialization of the
  materials or a use that generates or is intended to generate income, revenue, sales or profit.
The above copyright notice and this license shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
  THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*******************************************************************************************************************/

#ifndef gen_h
#define gen_h

/**DSP HELPER FUNCTIONS LIFTED STRAIGHT FROM GEN***/

/*
 TODO: Replace all with GPL-code
 */

extern inline t_sample linear_interp(t_sample a, t_sample x, t_sample y) {
    return x+a*(y-x);
}

extern inline t_sample phasewrap(t_sample val) {
    const t_sample twopi = TWOPI;
    const t_sample oneovertwopi = 1./twopi;
    if (val>= twopi || val <= twopi) {
        t_sample d = val * oneovertwopi;    //multiply faster
        d = d - (long)d;
        val = d * twopi;
    }
    if (val > PI) val -= twopi;
    if (val < -PI) val += twopi;
    return val;
}

extern inline t_sample genlib_cosT8(t_sample r) {
    const t_sample t84 = 56.;
    const t_sample t83 = 1680.;
    const t_sample t82 = 20160.;
    const t_sample t81 = 2.4801587302e-05;
    const t_sample t73 = 42.;
    const t_sample t72 = 840.;
    const t_sample t71 = 1.9841269841e-04;
    if (r < (PI/4.) && r > -(PI/4)){
        t_sample rr = r*r;
        return (t_sample)(1. - rr * t81 * (t82 - rr * (t83 - rr * (t84 - rr))));
    }
    else if (r > 0.){
        r -= (PI/2);
        t_sample rr = r*r;
        return (t_sample)(-r * (1. - t71 * rr * (t72 - rr * (t73 - rr))));
    }
    else {
        r += (PI/2);
        t_sample rr = r*r;
        return (t_sample)(r * (1. - t71 * rr * (t72 - rr * (t73 - rr))));
    }
}

extern inline t_sample genlib_cosT8_safe(t_sample r) {
    return genlib_cosT8(phasewrap(r));
}

extern inline t_sample cosine_interp(t_sample a, t_sample x, t_sample y) {
    const t_sample a2 = (1.-genlib_cosT8_safe(a*PI))/2.;
    return(x*(1.- a2)+y*a2);
}

#endif /* gen_h */
