void timemdel(unsigned int cons)
{
  T0CTCR = 0x0;
  T0PR = 59999;
  T0MR0 = cons;
  T0MCR |= 1 << 2;
  T0TCR = 0x02;
  T0TCR = 0x01;
  
  while (T0TC != T0MR0);
  
  T0TC = 0;
  T0TCR = 0;
}

void timeudel(unsigned int cons)
{
  T1CTCR = 0x0;
  T1PR = 59;
  T1MR0 = cons;
  T1MCR |= 1 << 2;
  T1TCR = 0x02;
  T1TCR = 0x01;
  
  while (T1TC != T1MR0);
  
  T1TC = 0;
  T1TCR = 0;
}
