#include "fitz.h"

/*

The functions in this file implement various flavours of Porter-Duff blending.

We take the following as definitions:

	Cx = Color (from plane x)
	ax = Alpha (from plane x)
	cx = Cx.ax = Premultiplied color (from plane x)

The general PorterDuff blending equation is:

	Blend Z = X op Y	cz = Fx.cx + Fy. cy	where Fx and Fy depend on op

The two operations we use in this file are: '(X in Y) over Z' and
'S over Z'. The definitions of the 'over' and 'in' operations are as
follows:

	For S over Z,	Fs = 1, Fz = 1-as
	For X in Y,	Fx = ay, Fy = 0

We have 2 choices; we can either work with premultiplied data, or non
premultiplied data. Our

First the premultiplied case:

	Let S = (X in Y)
	Let R = (X in Y) over Z = S over Z

	cs	= cx.Fx + cy.Fy	(where Fx = ay, Fy = 0)
		= cx.ay
	as	= ax.Fx + ay.Fy
		= ax.ay

	cr	= cs.Fs + cz.Fz	(where Fs = 1, Fz = 1-as)
		= cs + cz.(1-as)
		= cx.ay + cz.(1-ax.ay)
	ar	= as.Fs + az.Fz
		= as + az.(1-as)
		= ax.ay + az.(1-ax.ay)

This has various nice properties, like not needing any divisions, and
being symmetric in color and alpha, so this is what we use. Because we
went through the pain of deriving the non premultiplied forms, we list
them here too, though they are not used.

Non Pre-multiplied case:

	Cs.as	= Fx.Cx.ax + Fy.Cy.ay	(where Fx = ay, Fy = 0)
		= Cx.ay.ax
	Cs	= (Cx.ay.ax)/(ay.ax)
		= Cx
	Cr.ar	= Fs.Cs.as + Fz.Cz.az	(where Fs = 1, Fz = 1-as)
		= Cs.as	+ (1-as).Cz.az
		= Cx.ax.ay + Cz.az.(1-ax.ay)
	Cr	= (Cx.ax.ay + Cz.az.(1-ax.ay))/(ax.ay + az.(1-ax-ay))

Much more complex, it seems. However, if we could restrict ourselves to
the case where we were always plotting onto an opaque background (i.e.
az = 1), then:

	Cr	= Cx.(ax.ay) + Cz.(1-ax.ay)
		= (Cx-Cz)*(1-ax.ay) + Cz	(a single MLA operation)
	ar	= 1

Sadly, this is not true in the general case, so we abandon this effort
and stick to using the premultiplied form.

*/

typedef unsigned char byte;

/* Blend a non-premultiplied color in mask over destination */

static inline void
fz_paintspancolor2(byte * restrict dp, byte * restrict mp, int w, byte *color)
{
	int sa = FZ_EXPAND(color[1]);
	int g = color[0];
	while (w--)
	{
		int ma = *mp++;
		ma = FZ_COMBINE(FZ_EXPAND(ma), sa);
		dp[0] = FZ_BLEND(g, dp[0], ma);
		dp[1] = FZ_BLEND(255, dp[1], ma);
		dp += 2;
	}
}

static inline void
fz_paintspancolor4(byte * restrict dp, byte * restrict mp, int w, byte *color)
{
	int sa = FZ_EXPAND(color[3]);
	int r = color[0];
	int g = color[1];
	int b = color[2];
	while (w--)
	{
		int ma = *mp++;
		ma = FZ_COMBINE(FZ_EXPAND(ma), sa);
		dp[0] = FZ_BLEND(r, dp[0], ma);
		dp[1] = FZ_BLEND(g, dp[1], ma);
		dp[2] = FZ_BLEND(b, dp[2], ma);
		dp[3] = FZ_BLEND(255, dp[3], ma);
		dp += 4;
	}
}

static inline void
fz_paintspancolorN(byte * restrict dp, byte * restrict mp, int n, int w, byte *color)
{
	int sa = FZ_EXPAND(color[n-1]);
	int k;
	n--;
	while (w--)
	{
		int ma = *mp++;
		ma = FZ_COMBINE(FZ_EXPAND(ma), sa);
		for (k = 0; k < n; k++)
			dp[k] = FZ_BLEND(color[k], dp[k], ma);
		dp[k] = FZ_BLEND(255, dp[k], ma);
		dp += n;
	}
}

void
fz_paintspancolor(byte * restrict dp, byte * restrict mp, int n, int w, byte *color)
{
	switch (n)
	{
	case 2: fz_paintspancolor2(dp, mp, w, color); break;
	case 4: fz_paintspancolor4(dp, mp, w, color); break;
	default: fz_paintspancolorN(dp, mp, n, w, color); break;
	}
}

/* Blend source in mask over destination */

static inline void
fz_paintspanmask2(byte * restrict dp, byte * restrict sp, byte * restrict mp, int w)
{
	while (w--)
	{
		int masa;
		int ma = *mp++;
		ma = FZ_EXPAND(ma);
		masa = FZ_COMBINE(sp[1], ma);
		masa = 255 - masa;
		masa = FZ_EXPAND(masa);
		*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
		sp++; dp++;
		*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
		sp++; dp++;
	}
}

static inline void
fz_paintspanmask4(byte * restrict dp, byte * restrict sp, byte * restrict mp, int w)
{
	while (w--)
	{
		int masa;
		int ma = *mp++;
		ma = FZ_EXPAND(ma);
		masa = FZ_COMBINE(sp[3], ma);
		masa = 255 - masa;
		masa = FZ_EXPAND(masa);
		*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
		sp++; dp++;
		*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
		sp++; dp++;
		*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
		sp++; dp++;
		*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
		sp++; dp++;
	}
}

static inline void
fz_paintspanmaskN(byte * restrict dp, byte * restrict sp, byte * restrict mp, int n, int w)
{
	while (w--)
	{
		int k = n;
		int masa;
		int ma = *mp++;
		ma = FZ_EXPAND(ma);
		masa = FZ_COMBINE(sp[n-1], ma);
		masa = 255-masa;
		masa = FZ_EXPAND(masa);
		while (k--)
		{
			*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
			sp++; dp++;
		}
	}
}

void
fz_paintspanmask(byte * restrict dp, byte * restrict sp, byte * restrict mp, int n, int w)
{
	switch (n)
	{
	case 2: fz_paintspanmask2(dp, sp, mp, w); break;
	case 4: fz_paintspanmask4(dp, sp, mp, w); break;
	default: fz_paintspanmaskN(dp, sp, mp, n, w); break;
	}
}

/* Blend source in constant alpha over destination */

static inline void
fz_paintspan2alpha(byte * restrict dp, byte * restrict sp, int w, int alpha)
{
	alpha = FZ_EXPAND(alpha);
	while (w--)
	{
		int masa = FZ_COMBINE(sp[1], alpha);
		*dp = FZ_BLEND(*sp, *dp, masa);
		dp++; sp++;
		*dp = FZ_BLEND(*sp, *dp, masa);
		dp++; sp++;
	}
}

static inline void
fz_paintspan4alpha(byte * restrict dp, byte * restrict sp, int w, int alpha)
{
	alpha = FZ_EXPAND(alpha);
	while (w--)
	{
		int masa = FZ_COMBINE(sp[3], alpha);
		*dp = FZ_BLEND(*sp, *dp, masa);
		sp++; dp++;
		*dp = FZ_BLEND(*sp, *dp, masa);
		sp++; dp++;
		*dp = FZ_BLEND(*sp, *dp, masa);
		sp++; dp++;
		*dp = FZ_BLEND(*sp, *dp, masa);
		sp++; dp++;
	}
}

static inline void
fz_paintspanNalpha(byte * restrict dp, byte * restrict sp, int n, int w, int alpha)
{
	alpha = FZ_EXPAND(alpha);
	while (w--)
	{
		int masa = FZ_COMBINE(sp[n-1], alpha);
		int k = n;
		while (k--)
		{
			*dp = FZ_BLEND(*sp++, *dp, masa);
			dp++;
		}
	}
}

/* Blend source over destination */

static inline void
fz_paintspan1(byte * restrict dp, byte * restrict sp, int w)
{
	while (w--)
	{
		int t = FZ_EXPAND(255 - sp[0]);
		*dp = *sp++ + FZ_COMBINE(*dp, t);
		dp ++;
	}
}

static inline void
fz_paintspan2(byte * restrict dp, byte * restrict sp, int w)
{
	while (w--)
	{
		int t = FZ_EXPAND(255 - sp[1]);
		*dp = *sp++ + FZ_COMBINE(*dp, t);
		dp++;
		*dp = *sp++ + FZ_COMBINE(*dp, t);
		dp++;
	}
}

static inline void
fz_paintspan4(byte * restrict dp, byte * restrict sp, int w)
{
	while (w--)
	{
		int t = FZ_EXPAND(255 - sp[3]);
		*dp = *sp++ + FZ_COMBINE(*dp, t);
		dp++;
		*dp = *sp++ + FZ_COMBINE(*dp, t);
		dp++;
		*dp = *sp++ + FZ_COMBINE(*dp, t);
		dp++;
		*dp = *sp++ + FZ_COMBINE(*dp, t);
		dp++;
	}
}

static inline void
fz_paintspanN(byte * restrict dp, byte * restrict sp, int n, int w)
{
	while (w--)
	{
		int k = n;
		int t = FZ_EXPAND(255 - sp[n-1]);
		while (k--)
		{
			*dp = *sp++ + FZ_COMBINE(*dp, t);
			dp++;
		}
	}
}

void
fz_paintspan(byte * restrict dp, byte * restrict sp, int n, int w, int alpha)
{
	if (alpha == 255)
	{
		switch (n)
		{
		case 1: fz_paintspan1(dp, sp, w); break;
		case 2: fz_paintspan2(dp, sp, w); break;
		case 4: fz_paintspan4(dp, sp, w); break;
		default: fz_paintspanN(dp, sp, n, w); break;
		}
	}
	else if (alpha > 0)
	{
		switch (n)
		{
		case 2: fz_paintspan2alpha(dp, sp, w, alpha); break;
		case 4: fz_paintspan4alpha(dp, sp, w, alpha); break;
		default: fz_paintspanNalpha(dp, sp, n, w, alpha); break;
		}
	}
}

/*
 * Pixmap blending functions
 */

void
fz_paintpixmap(fz_pixmap *dst, fz_pixmap *src, int alpha)
{
	unsigned char *sp, *dp;
	fz_bbox bbox;
	int x, y, w, h, n;

	assert(dst->n == src->n);

	bbox = fz_boundpixmap(dst);
	bbox = fz_intersectbbox(bbox, fz_boundpixmap(src));

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;
	if ((w | h) == 0)
		return;

	n = src->n;
	sp = src->samples + ((y - src->y) * src->w + (x - src->x)) * src->n;
	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;

	while (h--)
	{
		fz_paintspan(dp, sp, n, w, alpha);
		sp += src->w * n;
		dp += dst->w * n;
	}
}

void
fz_paintpixmapmask(fz_pixmap *dst, fz_pixmap *src, fz_pixmap *msk)
{
	unsigned char *sp, *dp, *mp;
	fz_bbox bbox;
	int x, y, w, h, n;

	assert(dst->n == src->n);
	assert(msk->n == 1);

	bbox = fz_boundpixmap(dst);
	bbox = fz_intersectbbox(bbox, fz_boundpixmap(src));
	bbox = fz_intersectbbox(bbox, fz_boundpixmap(msk));

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;
	if ((w | h) == 0)
		return;

	n = src->n;
	sp = src->samples + ((y - src->y) * src->w + (x - src->x)) * src->n;
	mp = msk->samples + ((y - msk->y) * msk->w + (x - msk->x)) * msk->n;
	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;

	while (h--)
	{
		fz_paintspanmask(dp, sp, mp, n, w);
		sp += src->w * n;
		dp += dst->w * n;
		mp += msk->w;
	}
}
