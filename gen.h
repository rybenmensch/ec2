#ifndef gen_h
#define gen_h

/***DSP HELPER FUNCTIONS LIFTED STRAIGHT FROM GEN***/

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
