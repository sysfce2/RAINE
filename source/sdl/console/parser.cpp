#include "z80/mz80help.h"
#include "68020/u020help.h"
#include <muParser.h>
#include <stdio.h>
#include "compat.h"
#include "dialogs/messagebox.h"
#include "parser.h"
#include "starhelp.h"
#include "console.h"
#include "raine.h" // ReadWord !
#include "cpumain.h"
#include <vector>

/* muParser is specialised in double numbers, so it lacks some basic integer
 * operations, but can be easily extended, so let's go... */

using namespace mu;
static value_type mod(value_type v1, value_type v2) { return int(rint(v1))%int(rint(v2)); }
static value_type rol(value_type v1, value_type v2) { return int(rint(v1))<<int(rint(v2)); }
static value_type ror(value_type v1, value_type v2) { return int(rint(v1))>>int(rint(v2)); }
static value_type Or(value_type v1, value_type v2) { return int(v1) | int(v2); }
static value_type And(value_type v1, value_type v2) { return int(v1) & int(v2); }
static value_type lor(value_type v1, value_type v2) { return int(v1) || int(v2); }
static value_type land(value_type v1, value_type v2) { return int(v1) && int(v2); }
static value_type Not(value_type v1) { return ~int(rint(v1)); }
static value_type LogNot(value_type v1) { return !int(rint(v1)); }

double sr, pc, a[8], d[8],za,zb,zc,zde,zf,zhl,iff;
static double param;
static std::vector<double> vec;
double frame;

void set_nb_scripts(int n) {
    vec.resize(n);
}

void set_script_param(int n,int myp) {
    vec.at(n) = myp;
}

void init_script_param(int n) {
    param = vec.at(n);
}

value_type peek(value_type fadr) {
  UINT32 adr = fadr;
  UINT8 *ptr = get_ptr(adr);
  if (!ptr) throw "this adr isn't in ram !";

  if ((get_cpu_id()>>4) == 1)
      return ptr[adr ^ 1];
  else
      return ptr[adr];
}

value_type dpeek(value_type fadr) {
  UINT32 adr = fadr;
  UINT8 *ptr = get_ptr(adr);
  if (!ptr) throw "this adr isn't in ram !";

  return ReadWord(&ptr[adr]);
}

value_type lpeek(value_type fadr) {
  UINT32 adr = fadr;
  UINT8 *ptr = get_ptr(adr);
  if (!ptr) throw "this adr isn't in ram !";

  return ReadLongSc(&ptr[adr]);
}

void get_regs(int cpu) {
    int num;
    switch (cpu >> 4) {
    case CPU_68010:
    case CPU_68000:
	for (int n=0; n<8; n++) {
	    a[n] = (long)s68000_areg[n];
	    d[n] = (long)s68000_dreg[n];
	}
	sr = s68000_sr;
	pc = s68000_read_pc;
	break;
    case CPU_Z80:
	num = cpu & 0xf;
	switch_cpu(cpu+1);
	switch_cpu(cpu);
	za = (Z80_context[num].z80af>>8);
	zf = (Z80_context[num].z80af&0xff);
	zb = (Z80_context[num].z80bc>>8);
	zc = (Z80_context[num].z80bc&0xff);
	zde = Z80_context[num].z80de;
	zhl = Z80_context[num].z80hl;
#ifdef MAME_Z80
	pc = Z80_context[num].pc.d;
	iff = Z80_context[num].iff1|(Z80_context[num].iff2<<1);
#else
	pc = Z80_context[num].z80pc;
	iff = Z80_context[num].z80iff;
#endif
	break;
#ifndef NO020
    case 3: // 68020
#ifdef USE_MUSASHI
	for (int n=0; n<8; n++) {
	    a[n] = m68k_get_reg(NULL,(m68k_register_t)(M68K_REG_A0+n));
	    d[n] = (long)m68k_get_reg(NULL,(m68k_register_t)(M68K_REG_D0+n));
	}
	sr = m68k_get_reg(NULL,M68K_REG_SR);
	pc = m68k_get_reg(NULL,M68K_REG_PC);
#else
	for (int n=0; n<8; n++) {
	    a[n] = (long)regs.regs[n+8];
	    d[n] = (long)regs.regs[n];
	}
	sr = regs.sr;
	pc = regs.pc;
#endif
	break;
#endif
    }
}

void set_regs(int cpu) {
    int num = cpu & 0xf;
    switch (cpu >> 4) {
    case CPU_68010:
    case CPU_68000:
	for (int n=0; n<8; n++) {
	    s68000_areg[n] = a[n];
	    s68000_dreg[n] = d[n];
	}
#if USE_MUSASHI < 2
	if ((cpu >> 4) == CPU_68010) {
	    s68010context.sr = sr;
	    s68010context.pc = pc;
	} else {
	    M68000_context[num].sr = s68000context.sr = sr;
#else
	    m68ki_set_sr_noint_nosp(sr);
#endif
	    s68000_pc = pc;
#if USE_MUSASHI < 2
	}
#endif
	break;
    case 2:
	Z80_context[num].z80af = (int(za)<<8)|int(zf);
	Z80_context[num].z80bc = (int(zb)<<8)|int(zc);
	Z80_context[num].z80de = int(zde);
	Z80_context[num].z80hl = int(zhl);
#ifdef MAME_Z80
	Z80_context[num].pc.d = pc;
	Z80_context[num].iff1 = ((int)iff) & 1;
	Z80_context[num].iff2 = ((int)iff) >> 1;
#else
	Z80_context[num].z80pc = pc;
	Z80_context[num].z80iff = iff;
#endif
	mz80SetContext(&Z80_context[num]);
	break;
#ifndef NO020
    case 3:
#ifdef USE_MUSASHI
	for (int n=0; n<8; n++) {
	    m68k_set_reg((m68k_register_t)(M68K_REG_A0+n), a[n]);
	    m68k_set_reg((m68k_register_t)(M68K_REG_D0+n), d[n]);
	}
	m68k_set_reg(M68K_REG_SR,sr);
	m68k_set_reg(M68K_REG_PC, pc);
#else
	for (int n=0; n<8; n++) {
	    regs.regs[n+8] = a[n];
	    regs.regs[n] = d[n];
	}
	regs.sr = sr;
	regs.pc = pc;
#endif
	break;
#endif
    }
}

static value_type alert(const char_type *msg_and_btns) {
  // muparser doesn't support more than 1 string for its arguments...
  // so we'll pass the message + the buttons in the same string, separated by
  // the char |
  string_type mystr = msg_and_btns;
  char *cmsg = (char*)mystr.c_str();
  char *btn = strchr(cmsg,'|');
  if (btn) {
    *btn++ = 0;
    return MessageBox("alert",cmsg,btn);
  }
  // otherwise it's just a message
  return MessageBox("alert",cmsg);
}

static int initialised = 0;
static mu::Parser p;
int parser_error;
static double afValBuf[100];
static int iVal = 0;

double* AddVariable(const char *a_szName, void *pUserData)
{

  afValBuf[iVal++] = 0;
  if (iVal>=99)
    throw mu::Parser::exception_type("Variable buffer overflow.");

  return &afValBuf[iVal-1];
}

int parse(char *orig)
{
//  using namespace mu;
  int res;
  char expr[1024];
  strcpy(expr,orig);
  parser_error = 0;

  /* Handling of bases is stupid in muparser, you can't make it to understand
   * hex numbers begining by 0x, so it's easier to convert everything before
   * calling it... */
  strlwr(expr);
  char *s = expr;
  while ((s = strstr(s,"0x"))) {
    s[1] = '$';
    strcpy(s,s+1);
  } // 0x -> $
  s = expr;
  while ((s = strchr(s,'$'))) {
    *s = 0;
    s++;
    int res = 0;
    while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f')) {
      int digit;
      if (*s >= 'a') digit = *s - 'a' + 10;
      else
	digit = *s - '0';
      res = res*16 + digit;
      s++;
    }
    char buff2[1024];
    snprintf(buff2,1024,"%s%d%s",expr,res,s);
    strcpy(expr,buff2); // obliged to use another string for the sprintf
  }

  try
  {
    if (!initialised) {
      p.DefineOprt( "%", mod, 6);
      p.DefineOprt( "<<", rol, 3);
      p.DefineOprt( ">>", ror, 3);
      p.DefineOprt( "|", Or, 3);
      p.DefineOprt( "&", And, 3);
      // There is a collision between && and & so we must define new operators with the same role as && and || !
      p.DefineOprt( "or", lor, 3);
      p.DefineOprt( "and", land, 3);
      p.DefineInfixOprt( "~", Not);
      p.DefineInfixOprt( "!", LogNot);
      p.DefineFun( "alert", alert, false);
      p.DefineFun( "dpeek", dpeek, false);
      p.DefineFun( "peek", peek, false);
      p.DefineFun( "lpeek", lpeek, false);
      p.SetVarFactory(AddVariable,&p);
      // p.DefineFun( _T("r"), reg, false);
      for (int n=0; n<8; n++) {
	  char name[3];
	  sprintf(name,"d%d",n);
	  p.DefineVar(name,&d[n]);
	  sprintf(name,"a%d",n);
	  p.DefineVar(name,&a[n]);
      }
      p.DefineVar("sr",&sr);
      // z80
      p.DefineVar("a",&za);
      p.DefineVar("b",&zb);
      p.DefineVar("c",&zc);
      p.DefineVar("de",&zde);
      p.DefineVar("f",&zf);
      p.DefineVar("hl",&zhl);
      p.DefineVar("iff",&iff);
      p.DefineVar("pc",&pc);
      p.DefineVar("param",&param);
      p.DefineVar("frame",&frame);
      initialised = 1;
    }
    p.SetExpr(expr);
    res = p.Eval();

    // STd::cout << p.Eval() << endl;
  }
  catch (mu::Parser::exception_type &e)
  {
    parser_error = 1;
    char msg[80];
    // apparently the e.GetMsg().c_str() is corrupted in the 2nd throw below,
    // so we must copy the string to a temp buffer instead of using it directly
    strncpy(msg, e.GetMsg().c_str(),80);
    msg[79] = 0;
#ifdef RAINE_DEBUG
    printf("console: %s\n",msg);
#endif
    throw msg;
  }
  return res;
}

