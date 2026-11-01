extern const unsigned stream_image[];
volatile unsigned result[12];
void main(void){volatile const unsigned*p=(volatile const unsigned*)0x01000000;result[1]=(unsigned)stream_image;result[2]=p[0];result[3]=p[0x20000/4];result[4]=p[0x100000/4];result[5]=p[0x110000/4];result[0]=0x600dcafe;}
