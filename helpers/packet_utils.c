// ********************* includes *****************************
#include<stdio.h>
#include<ctype.h>


// ************************



char* encode(char* info) {

}

char* decode(char* info) {

}

void print_smth(char* info) {   //Ir jau pieejamas divas funkcijas šim.

}

void print_bytes(void* packet, int count);

unsigned char printable_char(unsigned char c){
  if(isprint(c)!=0) return c;
  return ' ';
}

void print_bytes(void* packet, int count){
  int i;
  unsigned char *p = (unsigned char*) packet;
  printf("Printing %d bytes...\n", count);
  printf("[NPK] [C] [HEX] [DEC] [BINARY]\n");
  printf("==============================\n");
  for(i=0; i<count; i++){
    printf(" %3d | %c |  %02X | %3d | %c%c%c%c%c%c%c%c\n",i,printable_char(p[i]), (unsigned char) p[i], (unsigned char) p[i],
      p[i] & 0x80 ? '1' : '0',
      p[i] & 0x40 ? '1' : '0',
      p[i] & 0x20 ? '1' : '0',
      p[i] & 0x10 ? '1' : '0',
      p[i] & 0x08 ? '1' : '0',
      p[i] & 0x04 ? '1' : '0',
      p[i] & 0x02 ? '1' : '0',
      p[i] & 0x01 ? '1' : '0');

  }
}

char* from_hex_to_dec(char* info) {

}

char* from_dec_to_hex(char* info) {
    
}

char* get_checksum(int packet) { // Kā padot paketi?

}

int main(){
  char c[10] = "Tests!";
  print_bytes(c,13);
}