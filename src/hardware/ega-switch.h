switch (bit_mask) {
	case 0:
	break;
	case 1:
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 2:
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 3:
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 4:
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 5:
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 6:
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 7:
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 8:
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 9:
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 10:
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 11:
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 12:
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 13:
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 14:
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 15:
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 16:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	break;
	case 17:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 18:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 19:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 20:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 21:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 22:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 23:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 24:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 25:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 26:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 27:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 28:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 29:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 30:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 31:
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 32:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	break;
	case 33:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 34:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 35:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 36:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 37:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 38:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 39:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 40:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 41:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 42:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 43:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 44:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 45:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 46:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 47:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 48:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	break;
	case 49:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 50:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 51:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 52:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 53:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 54:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 55:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 56:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 57:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 58:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 59:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 60:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 61:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 62:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 63:
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 64:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	break;
	case 65:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 66:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 67:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 68:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 69:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 70:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 71:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 72:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 73:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 74:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 75:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 76:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 77:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 78:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 79:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 80:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	break;
	case 81:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 82:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 83:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 84:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 85:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 86:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 87:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 88:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 89:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 90:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 91:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 92:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 93:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 94:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 95:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 96:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	break;
	case 97:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 98:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 99:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 100:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 101:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 102:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 103:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 104:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 105:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 106:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 107:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 108:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 109:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 110:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 111:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 112:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	break;
	case 113:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 114:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 115:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 116:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 117:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 118:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 119:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 120:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 121:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 122:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 123:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 124:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 125:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 126:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 127:
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 128:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	break;
	case 129:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 130:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 131:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 132:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 133:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 134:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 135:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 136:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 137:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 138:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 139:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 140:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 141:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 142:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 143:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 144:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	break;
	case 145:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 146:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 147:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 148:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 149:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 150:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 151:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 152:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 153:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 154:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 155:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 156:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 157:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 158:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 159:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 160:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	break;
	case 161:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 162:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 163:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 164:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 165:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 166:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 167:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 168:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 169:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 170:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 171:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 172:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 173:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 174:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 175:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 176:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	break;
	case 177:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 178:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 179:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 180:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 181:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 182:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 183:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 184:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 185:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 186:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 187:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 188:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 189:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 190:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 191:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 192:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	break;
	case 193:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 194:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 195:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 196:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 197:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 198:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 199:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 200:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 201:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 202:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 203:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 204:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 205:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 206:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 207:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 208:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	break;
	case 209:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 210:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 211:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 212:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 213:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 214:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 215:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 216:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 217:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 218:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 219:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 220:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 221:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 222:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 223:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 224:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	break;
	case 225:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 226:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 227:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 228:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 229:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 230:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 231:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 232:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 233:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 234:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 235:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 236:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 237:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 238:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 239:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 240:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	break;
	case 241:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 242:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 243:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 244:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 245:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 246:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 247:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 248:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	break;
	case 249:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 250:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 251:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 252:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	break;
	case 253:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
	case 254:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	break;
	case 255:
	{
		Bit8u color=0;
		if (pixels.b[0] & 128) color|=1;
		if (pixels.b[1] & 128) color|=2;
		if (pixels.b[2] & 128) color|=4;
		if (pixels.b[3] & 128) color|=8;
		*(write_pixels+0)=color;
		*(write_pixels+0+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 64) color|=1;
		if (pixels.b[1] & 64) color|=2;
		if (pixels.b[2] & 64) color|=4;
		if (pixels.b[3] & 64) color|=8;
		*(write_pixels+1)=color;
		*(write_pixels+1+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 32) color|=1;
		if (pixels.b[1] & 32) color|=2;
		if (pixels.b[2] & 32) color|=4;
		if (pixels.b[3] & 32) color|=8;
		*(write_pixels+2)=color;
		*(write_pixels+2+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 16) color|=1;
		if (pixels.b[1] & 16) color|=2;
		if (pixels.b[2] & 16) color|=4;
		if (pixels.b[3] & 16) color|=8;
		*(write_pixels+3)=color;
		*(write_pixels+3+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 8) color|=1;
		if (pixels.b[1] & 8) color|=2;
		if (pixels.b[2] & 8) color|=4;
		if (pixels.b[3] & 8) color|=8;
		*(write_pixels+4)=color;
		*(write_pixels+4+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 4) color|=1;
		if (pixels.b[1] & 4) color|=2;
		if (pixels.b[2] & 4) color|=4;
		if (pixels.b[3] & 4) color|=8;
		*(write_pixels+5)=color;
		*(write_pixels+5+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 2) color|=1;
		if (pixels.b[1] & 2) color|=2;
		if (pixels.b[2] & 2) color|=4;
		if (pixels.b[3] & 2) color|=8;
		*(write_pixels+6)=color;
		*(write_pixels+6+512*1024)=color;
	}
	{
		Bit8u color=0;
		if (pixels.b[0] & 1) color|=1;
		if (pixels.b[1] & 1) color|=2;
		if (pixels.b[2] & 1) color|=4;
		if (pixels.b[3] & 1) color|=8;
		*(write_pixels+7)=color;
		*(write_pixels+7+512*1024)=color;
	}
	break;
}
