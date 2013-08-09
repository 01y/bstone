// ID_VH.C

#include "ID_HEADS.H"


void VL_LatchToScreen(Uint16 source, Sint16 width, Sint16 height, Sint16 x, Sint16 y);
void IN_StartAck(void);
boolean IN_CheckAck (void);
void CalcTics (void);
void ForceUpdateStatusBar(void);


#define	SCREENWIDTH		80
#define CHARWIDTH		2
#define TILEWIDTH		4
#define GRPLANES		4
#define BYTEPIXELS		4

#define SCREENXMASK		(~3)
#define SCREENXPLUS		(3)
#define SCREENXDIV		(4)

#define VIEWWIDTH		80

#define PIXTOBLOCK		4		// 16 pixels to an update block

//#define UNCACHEGRCHUNK(chunk)	{MM_FreePtr(&grsegs[chunk]);grneeded[chunk]&=~ca_levelbit;}

//Uint8	update[UPDATEHIGH][UPDATEWIDE];	// MIKE this is the second declaration for this variable!?!?

//==========================================================================

pictabletype	*pictable;
pictabletype   *picmtable;			


Sint16	px,py;
Uint8	fontcolor,backcolor;
Sint16	fontnumber;
Sint16 bufferwidth,bufferheight;
boolean allcaps = false;


//==========================================================================

void	VWL_UpdateScreenBlocks (void);

//==========================================================================

void VW_DrawPropString (char *string)
{
	fontstruct	*font;
	Sint16		width,step,height,i;
	Uint8	*source, *dest, *origdest;
	Uint8	ch,mask;

	font = (fontstruct *)grsegs[STARTFONT+fontnumber];
	height = bufferheight = font->height;

    // FIXME
	dest = origdest = (Uint8*)0xA00000 + bufferofs + ylookup[py] + (px >> 2);

	mask = 1<<(px&3);


	while ((ch = *string++)!=0)
	{
		width = step = font->width[ch];
		source = ((Uint8 *)font)+font->location[ch];
		while (width--)
		{
			VGAMAPMASK(mask);

// FIXME
#if 0
asm	mov	ah,[BYTE PTR fontcolor]
asm	mov	bx,[step]
asm	mov	cx,[height]
asm	mov	dx,[linewidth]
asm	lds	si,[source]
asm	les	di,[dest]

vertloop:
asm	mov	al,[si]
asm	or	al,al
asm	je	next
asm	mov	[es:di],ah			// draw color

next:
asm	add	si,bx
asm	add	di,dx
asm	loop	vertloop
asm	mov	ax,ss
asm	mov	ds,ax
#endif // 0

			source++;
			px++;
			mask <<= 1;
			if (mask == 16)
			{
				mask = 1;
				dest++;
			}
		}
	}
bufferheight = height;
bufferwidth = ((dest+1)-origdest)*4;
}


//==========================================================================

#if 0

/*
=================
=
= VL_MungePic
=
=================
*/

void VL_MungePic (Uint8 *source, unsigned width, unsigned height)
{
	unsigned	x,y,plane,size,pwidth;
	Uint8		*temp, *dest, *srcline;

	size = width*height;

	if (width&3)
		VH_ERROR(VL_MUNGEPIC_NO_DIV_FOUR);

//
// copy the pic to a temp buffer
//
	MM_GetPtr (&(memptr)temp,size);
	_fmemcpy (temp,source,size);

//
// munge it back into the original buffer
//
	dest = source;
	pwidth = width/4;

	for (plane=0;plane<4;plane++)
	{
		srcline = temp;
		for (y=0;y<height;y++)
		{
			for (x=0;x<pwidth;x++)
				*dest++ = *(srcline+x*4+plane);
			srcline+=width;
		}
	}

	MM_FreePtr (&(memptr)temp);
}

#endif

void VWL_MeasureString (char *string, Uint16 *width, Uint16 *height
	, fontstruct *font)
{
	*height = font->height;
	for (*width = 0;*string;string++)
		*width += font->width[*((Uint8 *)string)];	// proportional width
}

void	VW_MeasurePropString (char *string, Uint16 *width, Uint16 *height)
{
	VWL_MeasureString(string,width,height,(fontstruct *)grsegs[STARTFONT+fontnumber]);
}

#if 0

void	VW_MeasureMPropString  (char *string, Uint16 *width, Uint16 *height)
{
	VWL_MeasureString(string,width,height,(fontstruct *)grsegs[STARTFONTM+fontnumber]);
}
#endif


/*
=============================================================================

				Double buffer management routines

=============================================================================
*/


/*
=======================
=
= VW_MarkUpdateBlock
=
= Takes a pixel bounded block and marks the tiles in bufferblocks
= Returns 0 if the entire block is off the buffer screen
=
=======================
*/

Sint16 VW_MarkUpdateBlock (Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
	Sint16	x,y,xt1,yt1,xt2,yt2,nextline;
	Uint8 *mark;

	xt1 = x1>>PIXTOBLOCK;
	yt1 = y1>>PIXTOBLOCK;

	xt2 = x2>>PIXTOBLOCK;
	yt2 = y2>>PIXTOBLOCK;

	if (xt1<0)
		xt1=0;
	else if (xt1>=UPDATEWIDE)
		return 0;

	if (yt1<0)
		yt1=0;
	else if (yt1>UPDATEHIGH)
		return 0;

	if (xt2<0)
		return 0;
	else if (xt2>=UPDATEWIDE)
		xt2 = UPDATEWIDE-1;

	if (yt2<0)
		return 0;
	else if (yt2>=UPDATEHIGH)
		yt2 = UPDATEHIGH-1;

	mark = updateptr + uwidthtable[yt1] + xt1;
	nextline = UPDATEWIDE - (xt2-xt1) - 1;

	for (y=yt1;y<=yt2;y++)
	{
		for (x=xt1;x<=xt2;x++)
			*mark++ = 1;			// this tile will need to be updated

		mark += nextline;
	}

	return 1;
}

void VWB_DrawTile8 (Sint16 x, Sint16 y, Sint16 tile)
{
	if (VW_MarkUpdateBlock (x,y,x+7,y+7))
		LatchDrawChar(x,y,tile);
}

#if 0

void VWB_DrawTile8M (int x, int y, int tile)
{
	if (VW_MarkUpdateBlock (x,y,x+7,y+7))
		VL_MemToScreen (((Uint8 *)grsegs[STARTTILE8M])+tile*64,8,8,x,y);
}
#endif


void VWB_DrawPic (Sint16 x, Sint16 y, Sint16 chunknum)
{
	Sint16	picnum = chunknum - STARTPICS;
	Uint16 width,height;

	x &= ~7;

	width = pictable[picnum].width;
	height = pictable[picnum].height;

	if (VW_MarkUpdateBlock (x,y,x+width-1,y+height-1))
		VL_MemToScreen (grsegs[chunknum],width,height,x,y);
}


//--------------------------------------------------------------------------
// VWB_DrawMPic()
//--------------------------------------------------------------------------
void VWB_DrawMPic (Sint16 x, Sint16 y, Sint16 chunknum)
{
	Sint16	picnum = chunknum - STARTPICS;
	Uint16 width,height;

	width = pictable[picnum].width;
	height = pictable[picnum].height;

	if (VW_MarkUpdateBlock (x,y,x+width-1,y+height-1))
		VL_MaskMemToScreen (grsegs[chunknum],width,height,x,y,255);
}


void VWB_DrawPropString	 (char *string)
{
	Sint16 x;
	x=px;
	VW_DrawPropString (string);
	VW_MarkUpdateBlock(x,py,px-1,py+bufferheight-1);
}


void VWB_Bar (Sint16 x, Sint16 y, Sint16 width, Sint16 height, Sint16 color)
{
	if (VW_MarkUpdateBlock (x,y,x+width,y+height-1) )
		VW_Bar (x,y,width,height,color);
}

void VWB_Plot (Sint16 x, Sint16 y, Sint16 color)
{
	if (VW_MarkUpdateBlock (x,y,x,y))
		VW_Plot(x,y,color);
}

void VWB_Hlin (Sint16 x1, Sint16 x2, Sint16 y, Sint16 color)
{
	if (VW_MarkUpdateBlock (x1,y,x2,y))
		VW_Hlin(x1,x2,y,color);
}

void VWB_Vlin (Sint16 y1, Sint16 y2, Sint16 x, Sint16 color)
{
	if (VW_MarkUpdateBlock (x,y1,x,y2))
		VW_Vlin(y1,y2,x,color);
}


#if 0		/// MADE A MACRO
void VW_UpdateScreen (void)
{
	VH_UpdateScreen ();
}
#endif


/*
=============================================================================

						WOLFENSTEIN STUFF

=============================================================================
*/

/*
=====================
=
= LatchDrawPic
=
=====================
*/


void LatchDrawPic (Uint16 x, Uint16 y, Uint16 picnum)
{
	Uint16 wide, height, source;

	x <<= 3;
	wide = pictable[picnum-STARTPICS].width;
	height = pictable[picnum-STARTPICS].height;
	source = latchpics[2+picnum-LATCHPICS_LUMP_START];

	if (VW_MarkUpdateBlock (x,y,x+wide-1,y+height-1))
		VL_LatchToScreen (source,wide/4,height,x,y);
}


//==========================================================================

/*
===================
=
= LoadLatchMem
=
===================
*/

//unsigned LatchMemFree = 0xffff;		
Uint16	destoff;

void LoadLatchMem (void)
{
	Sint16	i,j,p,m,width,height;
	Uint8	*src;
	Uint16	picnum=0;


//
// tile 8s
//
	latchpics[picnum++] = freelatch;
	CA_CacheGrChunk (STARTTILE8);
	src = (Uint8 *)grsegs[STARTTILE8];
	destoff = freelatch;

	for (i=0;i<NUMTILE8;i++)
	{
		VL_MemToLatch (src,8,8,destoff);
		src += 64;
		destoff +=16;
	}
	UNCACHEGRCHUNK (STARTTILE8);

#if 0	// ran out of latch space!
//
// tile 16s
//
	src = (Uint8 *)grsegs[STARTTILE16];
	latchpics[picnum++] = destoff;

	for (i=0;i<NUMTILE16;i++)
	{
		CA_CacheGrChunk (STARTTILE16+i);
		src = (Uint8 *)grsegs[STARTTILE16+i];
		VL_MemToLatch (src,16,16,destoff);
		destoff+=64;
		if (src)
			UNCACHEGRCHUNK (STARTTILE16+i);
	}
#endif

//
// pics
//
	picnum++;
	for (i=LATCHPICS_LUMP_START;i<=LATCHPICS_LUMP_END;i++)	  
	{
		latchpics[picnum++] = destoff;
		CA_CacheGrChunk (i);
		width = pictable[i-STARTPICS].width;
		height = pictable[i-STARTPICS].height;
		VL_MemToLatch (grsegs[i],width,height,destoff);
		destoff += width/4 *height;
		UNCACHEGRCHUNK(i);
	}

	EGAMAPMASK(15);
}


//==========================================================================


/*
===================
=
= FizzleFade
=
= returns true if aborted
=
===================
*/

extern	ControlInfo	c;

boolean FizzleFade (Uint16 source, Uint16 dest,
	Uint16 width,Uint16 height, Uint16 frames, boolean abortable)
{
	Sint16			pixperframe;
	Uint16	drawofs,pagedelta;
	Uint8 		mask,maskb[8] = {1,2,4,8};
	Uint16	x,y,p,frame;
	Sint32		rndval;

	pagedelta = dest-source;
	rndval = 1;
	y = 0;
	pixperframe = 64000/frames;

	IN_StartAck ();

	TimeCount=frame=0;
	do	// while (1)
	{
		if (abortable && IN_CheckAck () )
			return true;

// FIXME
#if 0
		asm	mov	es,[screenseg]

		for (p=0;p<pixperframe;p++)
		{
			//
			// seperate random value into x/y pair
			//
			asm	mov	ax,[WORD PTR rndval]
			asm	mov	dx,[WORD PTR rndval+2]
			asm	mov	bx,ax
			asm	dec	bl
			asm	mov	[BYTE PTR y],bl			// low 8 bits - 1 = y xoordinate
			asm	mov	bx,ax
			asm	mov	cx,dx
			asm	mov	[BYTE PTR x],ah			// next 9 bits = x xoordinate
			asm	mov	[BYTE PTR x+1],dl
			//
			// advance to next random element
			//
			asm	shr	dx,1
			asm	rcr	ax,1
			asm	jnc	noxor
			asm	xor	dx,0x0001
			asm	xor	ax,0x2000
noxor:
			asm	mov	[WORD PTR rndval],ax
			asm	mov	[WORD PTR rndval+2],dx

			if (x>width || y>height)
				continue;
			drawofs = source+ylookup[y] + (x>>2);

			//
			// copy one pixel
			//
			mask = x&3;
			VGAREADMAP(mask);
			mask = maskb[mask];
			VGAMAPMASK(mask);

			asm	mov	di,[drawofs]
			asm	mov	al,[es:di]
			asm add	di,[pagedelta]
			asm	mov	[es:di],al

			if (rndval == 1)		// entire sequence has been completed
				return false;
		}
#endif // 0

		frame++;
		while (TimeCount<frame)		// don't go too fast
		;

		CalcTics();
		if (!(frame&3))
			ForceUpdateStatusBar();

	} while (1);
}

