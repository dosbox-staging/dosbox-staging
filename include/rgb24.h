#ifndef DOSBOX_RGB24_H
#define DOSBOX_RGB24_H

typedef struct rgb24
{	
	protected:
	unsigned char byte[3];
	
	public:
	rgb24() {}
	rgb24(const rgb24& val) {
		*this = val; }

	operator int () const {
		return (byte[2]<<16)|(byte[1]<<8)|(byte[0]<<0); }
	int operator& (const rgb24& val) const	{
		return (char)*this & (char)val; }
} rgb24;

#endif
