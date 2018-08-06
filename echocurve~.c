#include "m_pd.h"
#include <stdlib.h>
#define UNUSED(x) (void)(x)
#define DEBUG(x) // debug off
//#define DEBUG(x) x // debug on
#define DEFDELVS 64             /* LATER get this from canvas at DSP time */
#define XTRASAMPS 4
#define SAMPBLK 4

static t_class *echocurve_tilde_class;

typedef struct _echocurve_tilde
{
        t_object x_obj;
        t_float x_buftime; /* max buffer length in msec */
        t_int x_bufsamps; /* max buffer length in msec */
        t_float x_deltime; /* delay in msec */
        t_int x_delsamps; /* delay in samples */
        t_float x_num_points;
        t_float x_dur_curve; //(0.0001-inf]
        t_float x_amp_curve; //(0.0001-inf]
        t_outlet *x_out;
        t_sample *x_outbuf;
        t_float x_srate;
        t_float x_f; // hello there, what am I good for?
        t_int x_bufindex;
} t_echocurve_tilde;

double experp (double inval, double inlo, double inhi, double curve, double outlo, double outhi)
{
        double lerp = (inval - inlo) / (inhi - inlo);
        double expval = pow (lerp, curve);
        return expval * (outhi - outlo) + outlo;
}

void *echocurve_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
        UNUSED(s);
        t_echocurve_tilde *x = (t_echocurve_tilde *)pd_new(echocurve_tilde_class);
        x->x_amp_curve = 1.;
        x->x_dur_curve = 1.;
        x->x_num_points = 20;
        x->x_buftime = 2000.;
        x->x_deltime = 2000.;
        x->x_srate = sys_getsr();
        switch (argc)
        {
        case 4:
                x->x_amp_curve = (t_float)argv[3].a_w.w_float;
                DEBUG(post("amp curve: %f", x->x_amp_curve); );
        case 3:
                x->x_dur_curve = (t_float)argv[2].a_w.w_float;
                DEBUG(post("dur curve: %f", x->x_dur_curve); );
        case 2:
                x->x_num_points = (t_int)argv[1].a_w.w_float;
                DEBUG(post("num points: %d", x->x_num_points); );
        case 1:
                x->x_buftime = (t_float)argv[0].a_w.w_float;
                x->x_deltime = (t_float)argv[0].a_w.w_float;
                DEBUG(post("buffer length (ms): %f", x->x_buftime); );
                break;
        case 0:
                error("echocurve~: no buffer length specified: defaulting to 2000 ms");
        }
        x->x_out = outlet_new(&x->x_obj, &s_signal);
        x->x_bufindex = 0;
        x->x_bufsamps = (t_int)(x->x_buftime / 1000. * x->x_srate);
        x->x_delsamps = (t_int)(x->x_deltime / 1000. * x->x_srate);
        x->x_outbuf = malloc(sizeof(t_sample) * x->x_bufsamps);
        return (void *)x;
}

void echocurve_tilde_free(t_echocurve_tilde *x)
{
        free(x->x_outbuf);
        outlet_free(x->x_out);
}

static t_int *echocurve_tilde_perform(t_int *w)
{
        t_echocurve_tilde *x = (t_echocurve_tilde *)(w[1]);
        t_sample *in = (t_sample*)(w[2]);
        t_sample *out = (t_sample*)(w[3]);
        int n = (int)(w[4]);
        unsigned int i,s;
        for (s = 0; s < n; s++)
        {
                for (i = 0; i < x->x_num_points; i++)
                {
                        double amp = experp (i, 0, x->x_num_points, x->x_amp_curve, 1, 0);
                        t_int point_index = (t_int)experp (i, 0, x->x_num_points, x->x_dur_curve, 0, x->x_delsamps);
                        //if (i < 5 && s==0) post("i: %d, point_index: %d", i, point_index);
                        t_int adjusted_point_index = ((point_index + x->x_bufindex) % x->x_delsamps);
                        //if (i < 5 && s==0) post("i: %d, adjusted_point_index: %d, x->x_bufindex: %d", i, adjusted_point_index, x->x_bufindex);
                        x->x_outbuf[adjusted_point_index] += (t_sample)(in[s]) * amp;
                        //if (i < 5 && s==0) post("i: %d, x->x_outbuf[adjusted_point_index]: %f", i, x->x_outbuf[adjusted_point_index]);
                }
                out[s] = x->x_outbuf[x->x_bufindex];
                x->x_outbuf[x->x_bufindex] = 0.;
                x->x_bufindex = ((x->x_bufindex + 1) % x->x_delsamps);
        }
        return (w+5);
}

static void echocurve_tilde_dsp(t_echocurve_tilde *x, t_signal **sp)
{
        dsp_add(echocurve_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
        if (x->x_buftime <= 0)
        {
                error("echocurve~: buffer duration must be greater than 0");
                x->x_buftime = 1000;
        }
}

void echocurve_tilde_float(t_echocurve_tilde *x, t_float f)
{
        x->x_deltime = f;
        if (x->x_deltime < 0) x->x_deltime = 0;
        if (x->x_deltime > x->x_buftime) x->x_deltime = x->x_buftime;
        x->x_delsamps = (t_int)(x->x_deltime * x->x_srate / 1000.);
        DEBUG(post("set x_deltime to %f",x->x_deltime); );
}

void echocurve_tilde_clear(t_echocurve_tilde *x)
{
        unsigned int i;
        for (i=0; i<x->x_bufsamps; i++) x->x_outbuf[i] = (t_sample)0.;
}

void echocurve_tilde_set_ampcurve(t_echocurve_tilde *x, t_float f)
{
        x->x_amp_curve = f;
        if (x->x_amp_curve <= 0)
        {
                error("echocurve~: ampcurve must be greater than 0.");
                x->x_amp_curve = 0.0001;
        }
}

void echocurve_tilde_set_durcurve(t_echocurve_tilde *x, t_float f)
{
        x->x_dur_curve = f;
        if (x->x_dur_curve <= 0)
        {
                error("echocurve~: durcurve must be greater than 0.");
                x->x_dur_curve = 0.0001;
        }
}

void echocurve_tilde_set_points(t_echocurve_tilde *x, t_float f)
{
        x->x_num_points = f;
        if (x->x_num_points <= 0)
        {
                error("echocurve~: points must be greater than 0.");
                x->x_num_points = 1;
        }
}

void echocurve_tilde_setup(void)
{
        echocurve_tilde_class = class_new(gensym("echocurve~"),
                                          (t_newmethod)echocurve_tilde_new,
                                          (t_method)echocurve_tilde_free,
                                          sizeof(t_echocurve_tilde),
                                          0, A_GIMME, 0);
        CLASS_MAINSIGNALIN(echocurve_tilde_class, t_echocurve_tilde, x_f);
        class_addfloat(echocurve_tilde_class, echocurve_tilde_float);
        class_addmethod(echocurve_tilde_class, (t_method)echocurve_tilde_clear, gensym("clear"), 0);
        class_addmethod(echocurve_tilde_class, (t_method)echocurve_tilde_set_ampcurve, gensym("ampcurve"), A_FLOAT, 0);
        class_addmethod(echocurve_tilde_class, (t_method)echocurve_tilde_set_durcurve, gensym("durcurve"), A_FLOAT, 0);
        class_addmethod(echocurve_tilde_class, (t_method)echocurve_tilde_set_points, gensym("reps"), A_FLOAT, 0);
        class_addmethod(echocurve_tilde_class, (t_method)echocurve_tilde_set_points, gensym("points"), A_FLOAT, 0);
        class_addmethod(echocurve_tilde_class, echocurve_tilde_dsp, gensym("dsp"), 0);
}
