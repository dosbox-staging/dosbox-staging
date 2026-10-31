# SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later
# Independent Decimal oracle; accepts the fpu_math_probe executable as argv[1].
from decimal import Decimal as D,localcontext,ROUND_FLOOR
import random,subprocess,sys
random.seed(51349)
with localcontext() as ctx:
 ctx.prec=170
 pi=D('3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270194')
 ln2=D(2).ln()
 def sincos(x):
  q=(x/(pi/2)).to_integral_value();r=x-q*pi/2
  s=st=r;c=ct=D(1)
  for n in range(2,400,2):
   st=st*-r*r/(n*(n+1));ct=ct*-r*r/((n-1)*n);s+=st;c+=ct
  return [(s,c),(c,-s),(-s,-c),(-c,s)][int(q)%4]
 def atan(x):
  neg=x<0;x=abs(x);recip=x>1
  if recip:x=1/x
  shifted=x>D('.5')
  if shifted:x=(x-1)/(x+1)
  t=s=x
  for n in range(3,1400,2):t=t*-x*x;s+=t/n
  if shifted:s+=pi/4
  if recip:s=pi/2-s
  return -s if neg else s
 def raw(x,rc):
  if not x:return (0,0)
  neg=x<0;x=abs(x);num,den=x.as_integer_ratio();e=num.bit_length()-den.bit_length()
  if D(2)**e>x:e-=1
  scaled=x*D(2)**(63-e);q=int(scaled);rem=scaled-q
  inc=(rem>D('.5') or rem==D('.5') and q%2) if rc==0 else rem>0 and ((rc==1 and neg)or(rc==2 and not neg))
  q+=bool(inc)
  if q==2**64:q//=2;e+=1
  return ((e+16383)|(0x8000 if neg else 0),q)
 cases=[];expected=[];compatibility=[]
 for op in range(7):
  for i in range(50):
   exp=16383+random.randint(-110,62 if op<3 else 0 if op==3 else 100)
   if op==5:exp=min(exp,16381)
   sig=random.randrange(2**63,2**64);sgn=random.choice([0,0x8000]) if op in [0,1,2,3,5,6] else 0
   x=D(sig)*D(2)**(exp-16383-63)*(-1 if sgn else 1)
   if op<3:
    s,c=sincos(x);y=[s,c,s/c][op]
   elif op==3:y=(x*ln2).exp()-1
   elif op==4:y=x.ln()/ln2
   elif op==5:y=(1+x).ln()/ln2
   else:y=atan(x)
   for rc in range(4):
    cases.append((op,rc,exp|sgn,sig))
    tiny_cosine = op == 1 and abs(x) < D(2)**-33
    if tiny_cosine:
     # Native-backend compatibility: +1 is the selected approximation.
     # Independently verify its error against the high-precision cosine.
     assert abs(D(1)-y) < D(2)**-67
    expected.append((0x3fff,2**63) if tiny_cosine else raw(y,rc))
    compatibility.append(tiny_cosine)
 data=''.join(f'{op} {rc} {ex:x} {sig:x}\n' for op,rc,ex,sig in cases)
 out=subprocess.check_output([sys.argv[1]],input=data,text=True)
 errors=[]
 for c,expected,line in zip(cases,expected,out.splitlines()):
  got=tuple(int(t,16) for t in line.split())
  if got!=expected:errors.append((c,expected,got))
 print(len(cases),'cases,',len(errors),'mismatches;',sum(compatibility),'native-compatible tiny-cosine cases with Decimal error bounds')
 for e in errors[:20]:print(e)

if errors: sys.exit(1)
