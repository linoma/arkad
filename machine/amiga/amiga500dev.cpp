#include "amiga500m.h"
#include <zlib.h>

namespace amiga{
/*
 * Task (4)+0x114 c0038a->c01446
 * c01446+10
 * c026e2+10 inpu.device
 * c07c28+10 filesysem
 * c06cd0		filesysem
 * c05f28		Filesysem
 * c0485e		trackdisk
 * c00a8
 */

static char _io_regs_name[][16]={"BLTDDAT","DMACONR","VPOSR","VHPOSR","DSKDATR","JOY0DAT","JOY1DAT",
	"CLXDAT","ADKCONR","POT0DAT","POT1DAT","POTGOR","SERDATR","DSKBYTR","INTENAR","INTREQR",
	"DSKPTH","DSKPTL","DSKLEN","DSKDAT","REFPTR","VPOSW","VHPOSW","COPCON","SERDAT","SERPER",
	"POTGO","JOYTEST","STREQU","STRVBL","STRHOR","STRLONG","BLTCON0","BLTCON1","BLTAFWM",
	"BLTALWM","BLTCPTH","BLTCPTL","BLTBPTH","BLTBPTL","BLTAPTH","BLTAPTL","BLTDPTH","BLTDPTL",
	"BLTSIZE","BLTCON0L","BLTSIZV","BLTSIZH","BLTCMOD","BLTBMOD","BLTAMOD","BLTDMOD","","","","","BLTCDAT",
	"BLTBDAT","BLTADAT","","","","DENISEID","DSKSYNC","COP1LCH","COP1LCL","COP2LCH","COP2LCL","COPJMP1",
	"COPJMP2","COPINS","DIWSTRT","DIWSTOP","DDF+STRT","DDFSTOP","DMACON","CLXCON","INTENA",
	"INTREQ","ADKCON","AUD0LCH","AUD0LCL","AUD0LEN","AUD0PER","AUD0VOL","AUD0DAT","","","AUD1LCH",
	"AUD1LCL","AUD1LEN","AUD1PER","AUD1VOL","AUD1DAT","","","AUD2LCH","AUD2LCL","AUD2LEN","AUD2PER",
	"AUD2VOL","AUD2DAT","","","AUD3LCH","AUD3LCL","AUD3LEN","AUD3PER","AUD3VOL","AUD3DAT","","","BPL1PTH",
	"BPL1PTL","BPL2PTH","BPL2PTL","BPL3PTH","BPL3PTL","BPL4PTH","BPL4PTL","BPL5PTH","BPL5PTL",
	"BPL6PTH","BPL6PTL","","","","","BPLCON0","BPLCON1","BPLCON2","BPLCON3","BPL1MOD","BPL2MOD","BPLCON4",
	"","BPL1DAT","BPL2DAT","BPL3DAT","BPL4DAT","BPL5DAT","BPL6DAT","BPL7DAT","BPL8DAT","SPR0PTH",
	"SPR0PTL","SPR1PTH","SPR1PTL","SPR2PTH","SPR2PTL","SPR3PTH","SPR3PTL","SPR4PTH","SPR4PTL",
	"SPR5PTH","SPR5PTL","SPR6PTH","SPR6PTL","SPR7PTH","SPR7PTL","SPR0POS","SPR0CTL","SPR0DATA",
	"SPR0DATB","SPR1POS","SPR1CTL","SPR1DATA","SPR1DATB","SPR2POS","SPR2CTL","SPR2DATA",
	"SPR2DATB","SPR3POS","SPR3CTL","SPR3DATA","SPR3DATB","SPR4POS","SPR4CTL","SPR4DATA",
	"SPR4DATB","SPR5POS","SPR5CTL","SPR5DATA","SPR5DATB","SPR6POS","SPR6CTL","SPR6DATA",
	"SPR6DATB","SPR7POS","SPR7CTL","SPR7DATA","SPR7DATB","COLOR00","COLOR01","COLOR02",
	"COLOR03","COLOR04","COLOR05","COLOR06","COLOR07","COLOR08","COLOR09","COLOR10","COLOR11",
	"COLOR12","COLOR13","COLOR14","COLOR15","COLOR16","COLOR17","COLOR18","COLOR19","COLOR20",
	"COLOR21","COLOR22","COLOR23","COLOR24","COLOR25","COLOR26","COLOR27","COLOR28","COLOR29",
	"COLOR30","COLOR31","BEAMCON0","DIWHIGH","FMODE"};



#define FLOPPY_DRIVE_HD
#define FLOPPY_WRITE_LEN 	(0 ? (12798 / 2) : (12668 / 2)) /* 12667 PAL, 12797 NTSC */
#define FLOPPY_WRITE_MAXLEN 0x3800
#define FLOPPY_GAP_LEN 		(FLOPPY_WRITE_LEN - 11 * 544)
#define NORMAL_FLOPPY_SPEED (0 ? 1810 : 1829)				/*currprefs.ntscmode*/
#define MAX_FLOPPY_DRIVES 	4
#define MAX_TRACKS 			(2 * 83)

#define IDEVICE_TRIGGER_PORT_WRITE	1000

amiga500dev::amiga500dev() : M68000Cpu(),Denise(),Paula(){
//	for(int i=0;i<sizeof(_io_regs_name)/16;i++)
//		printf("%03d %02x %s\n",i,i*2,_io_regs_name[i]);
}

amiga500dev::~amiga500dev(){
}

int amiga500dev::Init(void *a,void *b){
	if(M68000Cpu::Init(a,0x4000))
		return -2;
	M68000Cpu::_ioreg=(u16 *)(u8 *)b;
	if(Denise::Init(a,b,M68000Cpu::_freq))
		return -3;
	if(Paula::Init(a,b,M68000Cpu::_freq))
		return -4;
	if(_agnus.init(0,a,b,M68000Cpu::_freq))
		return -6;
	for(int i=0;i<sizeof(_cia)/sizeof(__cia);i++){
		if(_cia[i].Init(i,a,b,M68000Cpu::_freq))
			return -7;
	}
	if(_fdc.Init(0,a,b,M68000Cpu::_freq))
		return -8;
	_cia[__cia::B].Connect(&_fdc,FLOPPY,0);
	_cia[__cia::B]._extdevices.fdc=&_fdc;
	_cia[__cia::A]._extdevices.keyboard=&_keyboard;

	_keyboard.Init(0,a,b,M68000Cpu::_freq);
	_rs232.Init(0,a,b,M68000Cpu::_freq);
	_potgo.Init(0,a,b,M68000Cpu::_freq);
	for(int i=0;i<sizeof(_joy)/sizeof(__mouse);i++)
		_joy[i].Init(i,a,b,M68000Cpu::_freq);
	return 0;
}

int amiga500dev::Reset(){
	memset(M68000Cpu::_ioreg,0,0x200);
	M68000Cpu::Reset();
	_agnus.reset();
	for(int i=0;i<sizeof(_cia)/sizeof(__cia);i++)
		_cia[i].reset();
	_fdc.reset();
	_keyboard.reset();
	_rs232.reset();
	_potgo.reset();
	for(int i=0;i<sizeof(_joy)/sizeof(__mouse);i++)
		_joy[i].reset();
	Denise::Reset();
	Paula::Reset();
	return 0;
}

int amiga500dev::_enterIRQ(int n,u32 pc){
	u16 v;
	u8 tbl[]={1,1,1,2,3,3,3,4,4,4,4,5,5,6};

	Resume();
	v = BV(n)|SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQ));
	ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQ) = SWAP16(v);
	ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQR) = ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQ);

	if(!((v=SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_INTENA))) & BV(n)) || !(v & 0x4000))
		return 1;
	//if(tbl[n]==4) EnterDebugMode();
	return M68000Cpu::_enterIRQ(tbl[n],pc);
}

int amiga500dev::Destroy(){
	Denise::Destroy();
	Paula::Destroy();
	return M68000Cpu::Destroy();
}

amiga500dev::__fat_agnus::__fat_agnus(){
	_ioreg=NULL;
	_mem=NULL;
}

int amiga500dev::__fat_agnus::reset(){
	_status=0;
	_copper.reset();
	_blitter.reset();
	return 0;
}

int amiga500dev::__fat_agnus::init(int,void *a,void *b,u32 f){
	_ioreg=(u16 *)b;
	_mem=(u8 *)a;
	_copper.init(0,a,b,f);
	_blitter.init(0,a,b,f);
	return 0;
}

int amiga500dev::__fat_agnus::write(u32 a,u16 v){
	switch((u8)SR(a,1)){
		case REG_COP1LCH:
		case REG_COP1LCL:
		case REG_COP2LCH:
		case REG_COP2LCL:
		case REG_COPJMP1:
		case REG_COPJMP2:
		case REG_COPCON:
			_copper.write(a,v);
		break;
		case REG_DMACON:
			_copper.write(a,v);
			_blitter.write(a,v);
		break;
		case REG_BLTCON0:
		case REG_BLTCON1:
		case REG_BLTSIZE:
		case REG_BLTSIZH:
			return _blitter.write(a,v);
		break;
	}
	return 1;//update register
}

int amiga500dev::__fat_agnus::read(u32,u16 *){
	return 0;
}

int amiga500dev::__fat_agnus::_dumpRegisters(char *p){
	char cc[200];

	sprintf(cc,"\nBLITTER E:%d S:%u B:%d N:%d %04X %04X\n",_blitter._enabled,_blitter._state,_blitter._busy,
		_blitter._nasty,SWAP16(ACHIPREG(REG_BLTCON0)),SWAP16(ACHIPREG(REG_BLTCON1)));
	strcat(p,cc);
	sprintf(cc," DPT:%08X APT:%08X BPT:%08X CPT:%08X\n",BELE32(ACHIPREG32_(_ioreg,REG_BLTDPTH)) & 0xffffff,
		BELE32(ACHIPREG32_(_ioreg,REG_BLTAPTH)) & 0xffffff,BELE32(ACHIPREG32_(_ioreg,REG_BLTBPTH)) & 0xffffff,
		BELE32(ACHIPREG32_(_ioreg,REG_BLTCPTH)) & 0xffffff
	);
	strcat(p,cc);
	p += strlen(p);
	return _copper._dumpRegisters(p);
}

amiga500dev::__fat_agnus::__blitter::__blitter(){
	_ioreg=NULL;
	_mem=NULL;
}

int amiga500dev::__fat_agnus::__blitter::init(int,void *a,void *b,u32 f){
	_ioreg=(u16 *)b;
	_mem=(u8 *)a;
	_freq=f;
	reset();
	return 0;
}

int amiga500dev::__fat_agnus::__blitter::reset(){
	_status=0;//fccb24
	_cycles=0;
	memset(_regs,0,sizeof(_regs));
	_delay=0;
	_cycles=__cycles;
	return 0;
}

int amiga500dev::__fat_agnus::__blitter::update(int cyc){
	if(!_busy && _enabled && (_changed & 2) != 0){
		u16 c=SWAP16(ACHIPREG(REG_DMACON));
		c |= DMACON_BBUSY|DMACON_BZERO;
		ACHIPREG(REG_DMACON)=SWAP16(c);
		ACHIPREG(REG_DMACONR)=SWAP16(c);
		_state=1;
		//_changed&= ~2;
		DLOG("BLT %x %x D:%x,%x %x %x %x %ux%u",SWAP16(ACHIPREG(REG_BLTCON0)),SWAP16(ACHIPREG(REG_BLTCON1)),
			BELE32(ACHIPREG32_(_ioreg,REG_BLTDPTH)) & 0xffffff,BELE32(ACHIPREG32_(_ioreg,REG_BLTCPTH)) & 0xffffff,
			SWAP16(ACHIPREG(REG_BLTADAT)),SWAP16(ACHIPREG(REG_BLTBDAT)),SWAP16(ACHIPREG(REG_BLTCDAT)),
			SWAP16(ACHIPREG(REG_BLTSIZH)),SWAP16(ACHIPREG(REG_BLTSIZV))
		);
		cyc=455;
	}
	if(!_enabled || !_state)
		return 0;
	if(_changed && !_busy){
		u32 ticks;
		//_enabled=SR(SWAP16(ACHIPREG(REG_DMACON)),6);
		_width=SWAP16(ACHIPREG(REG_BLTSIZH));
		_height=SWAP16(ACHIPREG(REG_BLTSIZV));
		_regs[CON0].v=SWAP16(ACHIPREG(REG_BLTCON0));
		_regs[CON1].v=SWAP16(ACHIPREG(REG_BLTCON1));

		if(_regs[CON1].v & 1)//line
			ticks=8;
		else{
			ticks=4;
			if(_regs[CON0].v & 0x400)
				ticks += 2;
			if((_regs[CON0].v & 0x300) == 0x300)
				ticks += 2;
		}
		_delay=(_width*_height) * ticks;
		_cycles=__cycles;
		_changed&=~1;
		_changed=0;
	}
	//printf("BLITTER %x %u %u\n",_state,cyc,_delay);
	switch(_state){
		case 1:
			_state=2;
			_busy=1;
			//_delay -= _delay > cyc ? cyc : _delay;
			EnterDebugMode(DEBUG_BREAK_DEVICE);
			if(_nasty) {
			//	if(_delay)
			//		cpu->Query(ICORE_QUERY_CPU_SLEEP,&_delay);
				_delay=0;
			}
			else if(_delay > 999)
				return _delay;
		case 2:
			_state=3;
			_regs[APTH].v32=BELE32(ACHIPREG32_(_ioreg,REG_BLTAPTH)) & (MS_RAM-2);
			_regs[BPTH].v32=BELE32(ACHIPREG32_(_ioreg,REG_BLTBPTH)) & (MS_RAM-2);
			_regs[CPTH].v32=BELE32(ACHIPREG32_(_ioreg,REG_BLTCPTH)) & (MS_RAM-2);
			_regs[DPTH].v32=BELE32(ACHIPREG32_(_ioreg,REG_BLTDPTH)) & (MS_RAM-2);

			for(int i=0;i<4;i++){
				switch(SR(_regs[APTH+i].v32,20)){
					case 0xf:
						_regs[APM+i].mem=(u8 *)&_mem[MI_BIOS];
					break;
					case 0xc:
						_regs[APM+i].mem=(u8 *)&_mem[MI_RAM2];
					break;
					default:
						_regs[APM+i].mem=(u8 *)_mem;
					break;
				}
			}

			_regs[ADAT].v=SWAP16(ACHIPREG(REG_BLTADAT));
			_regs[BDAT].v=SWAP16(ACHIPREG(REG_BLTBDAT));
			_regs[CDAT].v=SWAP16(ACHIPREG(REG_BLTCDAT));

			_regs[AFWM].v=SWAP16(ACHIPREG(REG_BLTAFWM));
			_regs[ALWM].v=SWAP16(ACHIPREG(REG_BLTALWM));

			_regs[AMOD].sv32 = (s32)(s16)SWAP16(ACHIPREG(REG_BLTAMOD)) & ~1;
			_regs[BMOD].sv32 = (s32)(s16)SWAP16(ACHIPREG(REG_BLTBMOD)) & ~1;
			_regs[CMOD].sv32 = (s32)(s16)SWAP16(ACHIPREG(REG_BLTCMOD)) & ~1;
			_regs[DMOD].sv32 = (s32)(s16)SWAP16(ACHIPREG(REG_BLTDMOD)) & ~1;

			switch(_regs[CON1].v&3){
				default:
					_line();
				break;
				case 0:
					_ascending();
				break;
				case 2:
					_descending();
				break;
			}

			ACHIPREG32_(_ioreg,REG_BLTAPTH)=BELE32(_regs[APTH].v32);
			ACHIPREG32_(_ioreg,REG_BLTBPTH)=BELE32(_regs[BPTH].v32);
			ACHIPREG32_(_ioreg,REG_BLTCPTH)=BELE32(_regs[CPTH].v32);
			ACHIPREG32_(_ioreg,REG_BLTDPTH)=BELE32(_regs[DPTH].v32);

			ACHIPREG(REG_BLTADAT) = SWAP16(_regs[ADAT].v);
			ACHIPREG(REG_BLTBDAT) = SWAP16(_regs[BDAT].v);
			ACHIPREG(REG_BLTCDAT) = SWAP16(_regs[CDAT].v);

			ACHIPREG(REG_BLTCON0) = SWAP16(_regs[CON0].v);
			ACHIPREG(REG_BLTCON1) = SWAP16(_regs[CON1].v);
		break;
	}
A:
	if(_delay)
		_delay -= _delay > cyc ? cyc : _delay;
	if(_delay <= cyc && _busy){
		u16 r__=SWAP16(ACHIPREG(REG_INTREQ));
		r__|=INTENA_BLIT;
		ACHIPREG(REG_INTREQ)=SWAP16(r__);
		ACHIPREG(REG_INTREQR)=SWAP16(r__);
		machine->OnEvent(_log2(INTENA_BLIT),3);
		//_enabled=0;
		_state=0;
		_busy=0;
		_delay=0;

		u16 c=SWAP16(ACHIPREG(REG_DMACON));
		c &= ~(DMACON_BBUSY|DMACON_BZERO);

		ACHIPREG(REG_DMACON) = SWAP16(c);
		ACHIPREG(REG_DMACONR) = SWAP16(c);
	}
	return _nasty ? _delay : 0;
}

int amiga500dev::__fat_agnus::__blitter::write(u32 a,u16 v){
	int res=1;

	switch((u8)SR(a,1)){
		case REG_BLTSIZE:{
			u16 c;

			if(!(c=SR(v,6) & 0x3ff))
				c=0x400;
			ACHIPREG(REG_BLTSIZV)=SWAP16(c);
			if(!(c = v & 0x3f))
				c = 0x40;
			ACHIPREG(REG_BLTSIZH)=SWAP16(c);
			_changed |=2;
			res=0;
			update(455);
		}
		break;
		case REG_DMACON:
			_enabled = (v & (DMACON_BLTEN|DMACON_DMAEN)) == (DMACON_BLTEN|DMACON_DMAEN) ? 1 : 0;
			_nasty=(v&DMACON_BLTPRI) ? 1:0;
			_changed |= 1;
			update(455);
		break;
		case REG_BLTCON0:
		case REG_BLTCON1:
		case REG_BLTSIZH:
			_changed |= 4;
		break;
	}
	return res;//0 not update register
}

int amiga500dev::__fat_agnus::__blitter::read(u32,u16 *){return -1;}

int amiga500dev::__fat_agnus::__blitter::_ascending(){
	u32 shifta,acca,accb,shiftb;

	shifta = (_regs[CON0].v >> 12) & 0xf;
	shiftb = (_regs[CON1].v >> 12) & 0xf;
	acca = accb = 0;
	for (int y = 0; y < _height; y++){
		for (int x = 0; x < _width; x++){
			u32 tempa,tempd =0;
			u16 abc0,abc1,abc2,abc3;

			if(_regs[CON0].v & 0x800){
				_regs[ADAT].v = SWAP16(*(u16 *)&_regs[APM].mem[_regs[APTH].v32 ]);
				_regs[APTH].v32 +=2;
			}
			if(_regs[CON0].v & 0x400){
				_regs[BDAT].v = SWAP16(*(u16 *)&_regs[BPM].mem[_regs[BPTH].v32 ]);
				_regs[BPTH].v32 +=2;
			}
			if(_regs[CON0].v & 0x200){
				_regs[CDAT].v = SWAP16(*(u16 *)&_regs[CPM].mem[_regs[CPTH].v32 ]);
				_regs[CPTH].v32 +=2;
			}
			tempa = _regs[ADAT].v;
			if (x == 0)
				tempa &= _regs[AFWM].v;
			if (x == _width - 1)
				tempa &= _regs[ALWM].v;

			acca = (acca << 16) | (tempa << (16 - shifta));
			accb = (accb << 16) | (_regs[BDAT].v << (16 - shiftb));

			abc0 = ((acca >> 17) & 0x4444) | ((accb >> 18) & 0x2222) | ((_regs[CDAT].v >> 3) & 0x1111);
			abc1 = ((acca >> 16) & 0x4444) | ((accb >> 17) & 0x2222) | ((_regs[CDAT].v >> 2) & 0x1111);
			abc2 = ((acca >> 15) & 0x4444) | ((accb >> 16) & 0x2222) | ((_regs[CDAT].v >> 1) & 0x1111);
			abc3 = ((acca >> 14) & 0x4444) | ((accb >> 15) & 0x2222) | ((_regs[CDAT].v >> 0) & 0x1111);
			for (int b = 0; b < 4; b++){
				u32 bit;

				tempd <<= 4;

				bit = (_regs[CON0].v >> (abc0 >> 12)) & 1;
				abc0 <<= 4;
				tempd |= bit << 3;

				bit = (_regs[CON0].v >> (abc1 >> 12)) & 1;
				abc1 <<= 4;
				tempd |= bit << 2;

				bit = (_regs[CON0].v >> (abc2 >> 12)) & 1;
				abc2 <<= 4;
				tempd |= bit << 1;

				bit = (_regs[CON0].v >> (abc3 >> 12)) & 1;
				abc3 <<= 4;
				tempd |= bit;
			}

			if(_regs[CON0].v & 0x100){
				*(u16 *)&_regs[DPM].mem[_regs[DPTH].v32 ]=SWAP16(tempd);
				_regs[DPTH].v32 +=2;
			}
		}

		if(_regs[CON0].v & 0x800)
			_regs[APTH].v32 += _regs[AMOD].sv32;
		if(_regs[CON0].v & 0x400)
			_regs[BPTH].v32 += _regs[BMOD].sv32;
		if(_regs[CON0].v & 0x200)
			_regs[CPTH].v32 += _regs[CMOD].sv32;
		if(_regs[CON0].v & 0x100)
			_regs[DPTH].v32 += _regs[DMOD].sv32;
	}
	return 0;
}

int amiga500dev::__fat_agnus::__blitter::_line(){
	u32 singlemode = (_regs[CON1].v & 0x2) ? 0x0000 : 0xffff;
	u32 singlemask = 0xffff;

	for(int i=0;i<_height;i++){
		u16 abc0, abc1, abc2, abc3;
		u32 tempa, tempb, tempd = 0;
		s32 dx, dy;

		if(_regs[CON0].v & 0x200){
			_regs[CDAT].v = SWAP16(*(u16 *)&_regs[CPM].mem[_regs[CPTH].v32 ]);
		}

		tempa = _regs[ADAT].v >> (_regs[CON0].v >> 12);
		tempa &= singlemask;
		singlemask &= singlemode;

		tempb = -((_regs[BDAT].v >> (_regs[CON1].v >> 12)) & 1);

		abc0 = ((tempa >> 1) & 0x4444) | (tempb & 0x2222) | ((_regs[CDAT].v >> 3) & 0x1111);
		abc1 = ((tempa >> 0) & 0x4444) | (tempb & 0x2222) | ((_regs[CDAT].v >> 2) & 0x1111);
		abc2 = ((tempa << 1) & 0x4444) | (tempb & 0x2222) | ((_regs[CDAT].v >> 1) & 0x1111);
		abc3 = ((tempa << 2) & 0x4444) | (tempb & 0x2222) | ((_regs[CDAT].v >> 0) & 0x1111);

		for (int b = 0; b < 4; b++){
			u32 bit;

			tempd <<= 4;
			bit = (_regs[CON0].v >> (abc0 >> 12)) & 1;
			abc0 <<= 4;
			tempd |= bit << 3;

			bit = (_regs[CON0].v >> (abc1 >> 12)) & 1;
			abc1 <<= 4;
			tempd |= bit << 2;

			bit = (_regs[CON0].v >> (abc2 >> 12)) & 1;
			abc2 <<= 4;
			tempd |= bit << 1;

			bit = (_regs[CON0].v >> (abc3 >> 12)) & 1;
			abc3 <<= 4;
			tempd |= bit;
		}

		*(u16 *)&_regs[DPM].mem[_regs[DPTH].v32 ]=SWAP16(tempd);
		if (_regs[CON1].v & 0x10){
			dx = (_regs[CON1].v & 0x4) ? -1 : 1;
			dy = 0;
		}
		else{
			dx = 0;
			dy = (_regs[CON1].v & 0x4) ? -1 : 1;
		}

		if ((_regs[CON1].v & 0x40)==0){
			_regs[APTH].v32 += _regs[AMOD].sv32;
			if (_regs[CON1].v & 0x10)
				dy = (_regs[CON1].v & 0x8) ? -1 : 1;
			else
				dx = (_regs[CON1].v & 0x8) ? -1 : 1;
		}
		else
			_regs[APTH].v32 += _regs[BMOD].sv32;

		if (dx){
			u32 temp = _regs[CON0].v + (dx << 12);
			_regs[CON0].v = (u16)temp;
			if (temp & 0x10000){
				_regs[CPTH].v32 += 2 * dx;
				_regs[DPTH].v32 += 2 * dx;
			}
		}

		if (dy){
			_regs[CPTH].v32 += dy * _regs[CMOD].sv32;
			_regs[DPTH].v32 += dy * _regs[CMOD].sv32;

			singlemask = 0xffff;
		}
		_regs[CON1].v = (_regs[CON1].v & ~0x40) | ((_regs[APTH].v >> 9) & 0x40);
		_regs[CON1].v += 0x1000;
	}
	return 0;
}

int amiga500dev::__fat_agnus::__blitter::_descending(){
	u32 shifta,acca,accb,shiftb,fille,filli;

	shifta = (_regs[CON0].v >> 12) & 0xf;
	shiftb = (_regs[CON1].v >> 12) & 0xf;
	fille = (_regs[CON1].v >> 4);
	filli = (_regs[CON1].v >> 3);

	acca = accb = 0;
	for (int y = 0; y < _height; y++){
		u32 fs = (_regs[CON1].v >> 2) & 1;

		for (int x = 0; x < _width; x++){
			u32 tempa,tempd =0;
			u16 abc0,abc1,abc2,abc3;

			if(_regs[CON0].v & 0x800){
				_regs[ADAT].v = SWAP16(*(u16 *)&_regs[APM].mem[_regs[APTH].v32 ]);
				_regs[APTH].v32 -=2;
			}
			if(_regs[CON0].v & 0x400){
				_regs[BDAT].v = SWAP16(*(u16 *)&_regs[BPM].mem[_regs[BPTH].v32 ]);
				_regs[BPTH].v32 -=2;
			}
			if(_regs[CON0].v & 0x200){
				_regs[CDAT].v = SWAP16(*(u16 *)&_regs[CPM].mem[_regs[CPTH].v32 ]);
				_regs[CPTH].v32 -=2;
			}
			tempa = _regs[ADAT].v;
			if (x == 0)
				tempa &= _regs[AFWM].v;
			if (x == _width - 1)
				tempa &= _regs[ALWM].v;

			acca = (acca >> 16) | (tempa << shifta);
			accb = (accb >> 16) | (_regs[BDAT].v << shiftb);

			abc0 = ((acca >> 1) & 0x4444) | ((accb >> 2) & 0x2222) | ((_regs[CDAT].v >> 3) & 0x1111);
			abc1 = ((acca >> 0) & 0x4444) | ((accb >> 1) & 0x2222) | ((_regs[CDAT].v >> 2) & 0x1111);
			abc2 = ((acca << 1) & 0x4444) | ((accb >> 0) & 0x2222) | ((_regs[CDAT].v >> 1) & 0x1111);
			abc3 = ((acca << 2) & 0x4444) | ((accb << 1) & 0x2222) | ((_regs[CDAT].v >> 0) & 0x1111);

			for (int b = 0; b < 4; b++){
				u32 pfs,bit;

				tempd >>= 4;
				bit = (_regs[CON0].v >> (abc3 & 0xf)) & 1;
				abc3 >>= 4;
				pfs = fs;
				fs ^= bit;
				bit ^= pfs & fille;
				bit |= pfs & filli;
				tempd |= bit << 12;

				bit = (_regs[CON0].v >> (abc2 & 0xf)) & 1;
				abc2 >>= 4;
				pfs = fs;
				fs ^= bit;
				bit ^= pfs & fille;
				bit |= pfs & filli;
				tempd |= bit << 13;

				bit = (_regs[CON0].v >> (abc1 & 0xf)) & 1;
				abc1 >>= 4;
				pfs = fs;
				fs ^= bit;
				bit ^= pfs & fille;
				bit |= pfs & filli;
				tempd |= bit << 14;

				bit = (_regs[CON0].v >> (abc0 & 0xf)) & 1;
				abc0 >>= 4;
				pfs = fs;
				fs ^= bit;
				bit ^= pfs & fille;
				bit |= pfs & filli;
				tempd |= bit << 15;
			}

			if(_regs[CON0].v & 0x100){
				*(u16 *)&_regs[DPM].mem[_regs[DPTH].v32 ] = SWAP16(tempd);
				_regs[DPTH].v32 -= 2;
			}
		}

		if(_regs[CON0].v & 0x800)
			_regs[APTH].v32 -= _regs[AMOD].sv32;
		if(_regs[CON0].v & 0x400)
			_regs[BPTH].v32 -= _regs[BMOD].sv32;
		if(_regs[CON0].v & 0x200)
			_regs[CPTH].v32 -= _regs[CMOD].sv32;
		if(_regs[CON0].v & 0x100)
			_regs[DPTH].v32 -= _regs[DMOD].sv32;
	}
	return 0;
}

amiga500dev::__fat_agnus::__copper::__copper(){
	_ioreg=NULL;
	_mem=NULL;
}

int amiga500dev::__fat_agnus::__copper::init(int,void *a,void *b,u32 f){
	_ioreg=(u16 *)b;
	_mem=(u8 *)a;
	_freq=f;
	reset();
	return 0;
}

int amiga500dev::__fat_agnus::__copper::reset(){
	_status=0;
	_pc=0;
	_wait.reset();
	_lc[0]=_lc[1]=0;
	_opcodes.clear();
	_cycles=__cycles;
	return 0;
}

int amiga500dev::__fat_agnus::__copper::update(int cyc){
	int lino=0;
	if(_changed){
		//if(!_enabled) _opcodes.clear();
		_lc[0]=(_lc[0]&0xff0000)|SWAP16(ACHIPREG(REG_COP1LCL));
		_lc[0]=((u16)_lc[0]) | (SL(SWAP16(ACHIPREG(REG_COP1LCH)),16) & 0xff0000);
		_lc[1]=(_lc[1]&0xff0000)|SWAP16(ACHIPREG(REG_COP2LCL));
		_lc[1]=((u16)_lc[1]) | (SL(SWAP16(ACHIPREG(REG_COP2LCH)),16) & 0xff0000);
		//_pc=_lc[0];
		//_state=0;
		_changed=0;
	}

	if(__line==0){
		_pc=_lc[0];
		_state=0;
		//printf("cop item  %u %u\n",_opcodes.size(),_wait.w[0].vv);
		_opcodes.clear();
	}
	if(!_enabled || _busy || _state==STOP || __line > 255) {
		_cycles=__cycles;
		return 1;
	}
	u32 d=__cycles >= _cycles ? __cycles - _cycles:_freq-_cycles + __cycles;
	_cycles=__cycles;

	for(u32 c=0;c<d;){
		u16 w[2];

		if(_state==WAIT){
			int vc=__line & (0x80|_wait.w[1].v.vv);
			if(vc < _wait.w[0].v.vv)
				c=d;
			else if((_wait.w[1].val & 0x8000)==0 && (SWAP16(ACHIPREG(REG_DMACONR)) & DMACON_BBUSY)){
				c=d;
				printf("wait blitter %u\n",__line);
			}
			else{
				vc=SR(c,1) & ((u8)_wait.w[1].val);
				if(vc >= _wait.w[0].v.hv){
					_state=0;
					goto A;
				}
				c+=8;
			}
			continue;
		}
A:
		//printf("%x\n",_pc);
		w[0]=*(u16 *)&_mem[_pc];
		ACHIPREG(REG_COPINS)=w[0];
		w[0]=SWAP16(w[0]);
		_pc+=2;
		c+=4;

		w[1]=SWAP16(*(u16 *)&_mem[_pc]);
		_pc+=2;
		c+=4;

		if(!(w[0] & 1)){
			u8 r=(u8)SR(w[0],1);
			switch(r){
				default:
					if(r >= 0x20){
						//printf("copper op PC:%x %u %u %x %x\n",_pc,c,__line,r,w[1]);
						_opcodes.push_back({(u16)SR(c,2),(u16)__line,r,SWAP16(w[1])});
					}
					else{
						_state=STOP;
						c=d;
					}
				break;
				case REG_COPJMP2:
					_pc=_lc[1];
					//_state=STOP;
					//	c=d;
				break;
				case REG_COPJMP1:
					_pc=_lc[0];
					//_state=STOP;
					//	c=d;
				break;
				case REG_COP1LCH:
				case REG_COP1LCL:
				case REG_COP2LCH:
				case REG_COP2LCL:
					_changed=1;
				//	c=d;
				break;
			}
		}
		else {
		//	DLOG("COPPER %x %x %x S:%x",_pc,w[0],w[1],_state);
			_wait.w[0].val= w[0];
			_wait.w[1].val= w[1];
			if(w[1] & 1){
				int vc=__line & (0x80|_wait.w[1].v.vv);
				printf("skip %x %u %u %x %x\n",_pc,vc,__line,_wait.w[0].v.vv,_wait.w[1].v.vv);
				_state=SKIP;
				_state=0;
				c += 8;
				_pc += 4;
				lino=1;
			}
			else
				_state=WAIT;
			if(_wait.w[0].val==0xffff && _wait.w[1].val==0xfffe){
				_state=STOP;
				c=d;
			}
		//	if(_state == WAIT) printf("C %x %x %x\n",w[0],w[1],_wait.w[0].v.vv);
		}
	}
	//if(lino)EnterDebugMode();
	return 0;
}

int amiga500dev::__fat_agnus::__copper::_dumpRegisters(char *p){
	char cc[200];
	int mode = *p;

	*p=0;
	sprintf(cc,"COPPER %c:%d %x %u\n PC:%08X LC0:%08X LC1:%08X\n",
		(SWAP16(ACHIPREG(REG_DMACON)) & (DMACON_COPEN|DMACON_DMAEN))==(DMACON_COPEN|DMACON_DMAEN) ? 49 : 48,
		_state,ACHIPREG(REG_COPINS),(u32)_opcodes.size(),_pc,_lc[0],_lc[1]);
	strcat(p,cc);
#ifdef _DEVELOP
	switch(mode){
		case 1:
			for(int n=0;n<2;n++){
				for(int i=0;i<512;i++){
					u16 w1,w=SWAP16(*(u16 *)&_mem[_lc[n]+i*4]);
					w1=SWAP16(*(u16 *)&_mem[_lc[n]+i*4+2]);
					sprintf(cc,"%c%06X ",_lc[n]+i*4==_pc ?'*':32,_lc[n]+i*4);
					strcat(p,cc);
					if(!(w&1)){
						if(w>0x1ff)
							sprintf(cc,"%04X=>%04X\n",w1,w/2);
						else
							sprintf(cc,"%04X=>%s\n",w1,_io_regs_name[w/2]);
					}
					else{
						if(!(w1&1)){
							sprintf(cc,"WAIT V:%02X H:%02X %02X %02X\n",
							SR(w,8),(u8)w,(u8)w1,SR(w1,8));
						}
						else{
							sprintf(cc,"SKIP V:%02X H;%02X %02X %02X\n",
							SR(w,8),(u8)w,(u8)w1,SR(w1,8));
						}
					}
					strcat(p,cc);
					if((w==0xffff && w1==0xfffe) || (w==0 && w1==0)) break;
				}
				strcat(p,"\n");
			}
		break;
		default:
			for(auto it=_opcodes.begin();it != _opcodes.end();it++){
				COPPERITEM ci =*it;

				sprintf(cc,"Y:%d X:%d \t%x->%x\n",ci._y,ci._x,ci._r*2,ci._val);
				strcat(p,cc);
			}
		break;
	}
#endif
	return 0;
}

int amiga500dev::__fat_agnus::__copper::write(u32 a,u16 v){
	switch((u8)SR(a,1)){
		case REG_DMACON:{
			u8 enabled=_enabled;

			_enabled=(SWAP16(ACHIPREG(REG_DMACON)) & (DMACON_DMAEN|DMACON_COPEN)) == (DMACON_DMAEN|DMACON_COPEN);
			if(!enabled && _enabled)
				_state=STOP;
			_changed=1;
		}
		break;
		case REG_COP1LCL:
		case REG_COP1LCH:
		case REG_COP2LCH:
		case REG_COP2LCL:
			_changed=1;
		//	if(_enabled && _state==STOP) _state=0;
		break;
		case REG_COPJMP1:
			_pc=_lc[0];
		break;
		case REG_COPJMP2:
			_pc=_lc[1];
		break;
	}
	return 0;
}

int amiga500dev::__fat_agnus::__copper::read(u32,u16 *){
	return 0;
}

amiga500dev::__cia::__cia(){
	_ioreg=NULL;
	_mem=NULL;
}

int amiga500dev::__cia::Connect(IDevice *,u32 devId,u32 portId){
	return 0;
}

int amiga500dev::__cia::Trigger(u32 what,u32 devId,u32 portId,void *b){
	switch(devId){
		case FLOPPY:
			switch(portId){
				case 0:
					machine->OnEvent((u8)(u64)b,1);
				break;
				default:
					enterIRQ((u8)(u64)b);
				break;
			}
		break;
		case KEYBOARD:
			enterIRQ((u8)(u64)b);
		break;
	}
	return 0;
}

int amiga500dev::__cia::reset(){
	memset(_regs,0,sizeof(_regs));
	_regs[TA_LO]=0xff;
	_regs[TA_HI]=0xff;
	_regs[TB_LO]=0xff;
	_regs[TB_HI]=0xff;
	_timers[0].reset();
	_timers[1].reset();
	_tod.reset();
	_sdr.reset();
	return 0;
}

int amiga500dev::__cia::enterIRQ(u8 v){
	_regs[ICR] |= v;
	if(!(_regs[IMR] & v))
		return 1;
	_regs[ICR] |= 0x80;
	machine->OnEvent(_log2(_idx ? INTENA_EXTER : INTENA_PORTS),1);
	return 0;
}

int amiga500dev::__cia::Init(int n,void *a,void *b,u32 freq){
	_ioreg=(u16 *)b;
	_mem=(u8 *)a;
	_idx=n;
	_sdr._idx=n;
	_sdr._freq=freq;
	_tod._sync=n;
	_tod._freq=freq;
	_timers[0]._freq=freq;
	_timers[0]._idx=0;
	_timers[1]._freq=freq;
	_timers[1]._idx=1;
	_regs[PRA]=0xc0;
	return 0;
}

int amiga500dev::__cia::write(u32 a,u16 v){
	u8 b;

	//DLOG("CIA W %d %x %x",_idx,SR(a&0xf00,8),v);
	switch((b=SR(a,8)&0xf)){
		case PRB:
			_regs[b]=v;
			if(_idx == B){
				_extdevices.fdc->Trigger(IDEVICE_TRIGGER_PORT_WRITE,CIAA,PRB,(void *)(u64)v);
				return 0;
			}
		break;
		case PRA:
			if(_idx==B){//rs232
			}
			else{
				u8 m=~(_regs[DDRA]&0xc0) | 0x3c;
				v = (v & ~m) | (_regs[PRA] & m);
			//	EnterDebugMode(DEBUG_BREAK_DEVICE);
			}
			_regs[PRA]=v;
		break;
		case CRA:
			_regs[b]=v&~0x90;
			_timers[0]._control._value=v;
			if(_timers[0]._control.a._load)
				_timers[0]._count=_timers[0]._load;
		break;
		case CRB:
			_regs[b]=v;
			_timers[1]._control._value=v;
			//if((v&0x80) && !_tod._ae) printf("alarn %x\n",_pc);
			_tod._ae=SR(v,7);
			_tod._changed=1;
			if(_timers[1]._control.b._load)
				_timers[1]._count=_timers[1]._load;
		break;
		case TA_LO:
			_timers[0]._load=(_timers[0]._load&0xff00)|v;
		break;
		case TA_HI:
			_timers[0]._load=(_timers[0]._load&0xff)|SL(v,8);
			if(_timers[0]._control.a._rmode){
				_timers[0].start();
				_regs[CRA] |= 1;
				//EnterDebugMode(DEBUG_BREAK_DEVICE);
			}
		break;
		case TB_LO:
			_timers[1]._load=(_timers[1]._load&0xff00)|v;
		break;
		case TB_HI:
			_timers[1]._load=(_timers[1]._load&0xff)|SL(v,8);
			if(_timers[1]._control.b._rmode){
				_timers[1].start();
				_regs[CRB] |= 1;
				//EnterDebugMode(DEBUG_BREAK_DEVICE);
			}
		break;
		case TOD_10THS:
			_regs[b]=v;
			_tod._changed=1;
			if(!(_regs[CRB] & 0x80)){
				_tod.enable(1);
				_tod._count=(_tod._count&0xffff00)|v;
				if(!_tod.alarm(_tod._count)) enterIRQ(4);
			}
			else
				_tod.reg[4] = v;
		break;
		case TOD_SEC:
			if(!(_regs[CRB] & 0x80))
				_tod._count=(_tod._count&0xff00ff)|SL(v,8);
			else
				_tod.reg[4+1] = v;
			_tod._changed=1;
		break;
		case TOD_MIN:
			_regs[b]=v;
			_tod._changed=1;
			if(!(_regs[CRB] & 0x80)){
				_tod.enable(0);
				_tod._count=(_tod._count&0xffff)|SL(v,16);
			}
			else
				_tod.reg[4+2]=v;
		break;
		case ICR:
			if(v & 0x80){
				_regs[IMR] |= v;
				if(_regs[ICR] & _regs[IMR]){
					machine->OnEvent(_log2(_idx ? INTENA_EXTER : INTENA_PORTS),1);
					_regs[ICR] |= 0x80;
				}
			}
			else
				_regs[IMR] &= ~v;
		break;
		case DDRA:
			_regs[b]=v;
		break;
	}
	return 0;
}

int amiga500dev::__cia::read(u32 a,u16 *m){
	u8 b;

	switch((b=SR(a,8)&0xf)){
		case TA_LO:
		case TA_HI:
			_timers[0].update();
			_regs[TA_LO]=(u8)_timers[0]._count;
			_regs[TA_HI]=SR(_timers[0]._count,8);
			*m=_regs[b];
		break;
		case TB_LO:
		case TB_HI:
			_timers[1].update();
			_regs[TB_LO]=(u8)_timers[1]._count;
			_regs[TB_HI]=SR(_timers[1]._count,8);
			*m=_regs[b];
		break;
		case TOD_10THS:
			_tod.update();
			_regs[TOD_10THS]=_tod._count;
			if(_tod._latch)
				_tod.latch(0);
			*m=_regs[b];
		break;
		case TOD_SEC:
			_tod.update();
			_regs[TOD_SEC]=SR(_tod._count,8);
			*m=_regs[b];
		break;
		case TOD_MIN:
			_tod.update();
			_regs[TOD_MIN]=SR(_tod._count,16);
			if(!_tod._latch){
				_tod.latch(1);
			}
			*m=_regs[b];
		break;
		case ICR:
			*m=_regs[ICR];
			_regs[ICR]=0;
			machine->OnEvent(_log2(_idx ? INTENA_EXTER : INTENA_PORTS),2);
			break;
		case SDR:
			if(_idx==A){
				_extdevices.keyboard->read(&_regs[SDR]);
			}
		default://PRA CIA1 serial/par status
			*m=_regs[b];
		break;
	}
	//DLOG("CIA R %d %x %x",_idx,SR(a,8)&15,*m);
	return 0;
}

int amiga500dev::__cia::dump(){
	_sdr.update();
	_timers[0].update();
	_timers[1].update();
	_tod.update();

	_regs[TA_LO]=(u8)_timers[0]._count;
	_regs[TA_HI]=SR(_timers[0]._count,8);
	_regs[TB_LO]=(u8)_timers[1]._count;
	_regs[TB_HI]=SR(_timers[1]._count,8);
	_regs[TOD_10THS]=_tod._count;
	_regs[TOD_SEC]=SR(_tod._count,8);
	_regs[TOD_MIN]=SR(_tod._count,16);
	_regs[TOD_HR]=SR(_tod._count,24);
	_regs[TOD_A10THS]=_tod._count;
	_regs[TOD_ASEC]=SR(_tod._alarm,8);
	_regs[TOD_AMIN]=SR(_tod._alarm,16);
	_regs[TOD_AHR]=SR(_tod._alarm,24);
	return 0;
}

int amiga500dev::__cia::update(int cyc){
	if(!_tod.update(cyc))
		enterIRQ(4);
	_sdr.update(cyc);
	cyc=0;
	if(!_timers[0].update()){
		enterIRQ(1);
		_regs[CRA] = _timers[0]._control._value;
		if(_timers[1]._control.a._start && _timers[1]._control.b._inmode){
			cyc=1;
			_timers[1]._cycles=__cycles-10;
		}
	}
	if(!_timers[1].update(cyc)){
		enterIRQ(2);
		_regs[CRB] =_timers[1]._control._value;
	}
A:
	return 0;
}

int amiga500dev::__cia::__timer::reset(){
	_control._value=0;
	_load=_count=0xffff;
	return 0;
}

int amiga500dev::__cia::__timer::update(int force){
	u16 count;

	if(!_control.a._start)
		return -1;
	if(_control.b._inmode) printf("%d %x\n",_idx,_control.b._inmode);
	if(_control.b._inmode && !force)
		return 1;
	u32 d=__cycles >= _cycles ? __cycles - _cycles:_freq-_cycles + __cycles;
	if(!(d/=10))
		return 1;
	_cycles=__cycles;
	count=_count;
	_count -= d;
	if(_count <= count)
		return 1;
	_count=_load;
	if(_control.c._rmode){
		_control.c._start=0;
		//_regs[_idx ? CRB : CRA] &= ~1;
	}
	return 0;
}

int amiga500dev::__cia::__timer::start(){
	_control.a._start=1;
	_count=_load;
	_cycles=__cycles;
	return 0;
}

int amiga500dev::__cia::__tod::reset(){
	_cycles=__cycles;
	_count=0;
	_enabled=0;
	_ae=0;
	_changed=0;
	*(u64 *)reg=0;
	return 0;
}

int amiga500dev::__cia::__tod::enable(int  v){
	_enabled=v;
	_changed=1;
	_cycles=__cycles;
	return 0;
}

int amiga500dev::__cia::__tod::update(int){
	u32 count;

	//cia a vsync
	//cia b hsync
	if(!_enabled)
		return -1;
	if(_changed){
		if(_ae){
			_alarm=*(u32 *)&reg[4];
		}
		_changed=0;
	}
	u32 d=__cycles >= _cycles ? __cycles-_cycles: (_freq-_cycles) + __cycles;
	if(!_sync)
		d /= 455*312/4;
	else
		d/= 64;
	if(!d)
		return 1;
	_cycles=__cycles;
	count=_count;
	_count = (_count + d) & 0xffffff;
	if(!alarm(count)) return 0;
	return 2;
}

int amiga500dev::__cia::__tod::latch(int v){
	if((_latch=v&1))
		_countl=_count;
	return 0;
}

int amiga500dev::__cia::__tod::alarm(u32 count){
	if(!_ae) return 1;
	if((_alarm & ~7) == (_count & ~7) || (_count < count && _alarm < _count))
		return 0;
	return -1;
}

int amiga500dev::__cia::__sdr::write(u8){
	return -1;
}

int amiga500dev::__cia::__sdr::reset(){
	_cycles=__cycles;
	return 0;
}

int amiga500dev::__cia::__sdr::update(int){
	u32 d=__cycles >= _cycles ? __cycles - _cycles:_freq-_cycles + __cycles;
	_cycles=__cycles;
	return 0;
}

amiga500dev::__fdc::__fdc() : ADevice(){
	reset();
}

amiga500dev::__fdc::~__fdc(){
}

int amiga500dev::__fdc::Init(int n,void *a,void *b,u32 f){
	void *r;

	ADevice::Init(n,a,b,f);

	cpu->Query(ICORE_QUERY_CPU_FREQ,&_freq);

	_ciabreg=(u8 *)CIAB;
	cpu->Query(ICORE_QUERY_IO_PORT,&_ciabreg);
	_ciab=(IDevice *)CIAB;
	cpu->Query(ICORE_QUERY_IO_DEVICE,&_ciab);

	_ciaareg=(u8 *)CIAA;
	cpu->Query(ICORE_QUERY_IO_PORT,&_ciaareg);
	_ciaa=(IDevice *)CIAA;
	cpu->Query(ICORE_QUERY_IO_DEVICE,&_ciaa);
	return reset();
}

int amiga500dev::__fdc::reset(){
	_status=0;
	for(int i=0;i<sizeof(_floppies)/sizeof(__floppy);i++)
		_floppies[i]._reset();
	_cycles=__cycles;
	memset(_regs,0,sizeof(_regs));
	_regs[DSKLEN]=0x4000;
	_regs[DSKSYNC]=0xffff;
	_floppy=NULL;
	_selected=0xf;
	return 0;
}

int amiga500dev::__fdc::update(int cyc){
	u32 d;

	d=(__cycles >= _cycles ? __cycles - _cycles : (_freq-_cycles) + __cycles);
	if(d < 1829)
		return 1;
	for(int i=0;i<sizeof(_floppies)/sizeof(__floppy);i++){
		__floppy *p=&_floppies[i];
		if(p->_changed){
			*(u32 *)&_regs[DSKPT]= BELE32(ACHIPREG32_(_ioreg,REG_DSKPTH)) & (MS_RAM-2);
		//	printf("DISKPT %x\n",_regs[DSKPT]);
			_floppies[i]._changed=0;
		}
		if(p->_cycles[2] > d)
			p->_cycles[2] -= d;
		else
			p->_cycles[2]=0;
		if(!p->_on)
			continue;

		if(!p->_selected){
			u32 dd;
		//	printf("%u %u %u %u %u ",i,d,p->_cycles,p->_speed,p->_len);
			p->_cycles[0] += d;
			p->_mfmpos = (p->_mfmpos + (dd=p->_cycles[0] / p->_speed )) % p->_len;
			p->_cycles[0] -= (dd*p->_speed);
			//printf("%u\n",p->_mfmpos);
		}
	}

	if(!_floppy || !_floppy->_on)
		goto Z;

	if(_floppy->_refill){
		//if(_floppy->_cyl*2+_side==105) EnterDebugMode();
		_floppy->_decode(_floppy->_cyl*2+_side,_floppy->_buf);
		_floppy->_refill=0;
	}

	if(_floppy->_turbo){
		u32 pos = _floppy->_mfmpos & ~0xf;
		if((_regs[DSKLEN] & 0x3fff)==0){
			_floppy->_cycles[1]+=_floppy->_speed*_floppy->_len;
			pos = (pos + _floppy->_len)%_floppy->_len;
		}
		else{
			for (;(_regs[DSKLEN] & 0x3fff) > 0;) {
				_regs[DMAVAL] = _floppy->_buf[pos >> 4];
				pos = (pos+16) % _floppy->_len;
				_write();
				_floppy->_cycles[1]+=16*_floppy->_speed;
			}
		}
		_floppy->_mfmpos = pos + (_floppy->_mfmpos & 0xf);
		goto X;
	}
	//printf("rea %u %u %u\n",d,_floppy->_cycles+d,_floppy->_speed);
	for(_floppy->_cycles[0] += d;_floppy->_cycles[0] >=_floppy->_speed;_floppy->_cycles[0] -=_floppy->_speed){
		int bit;

		_regs[DMAREG]<<=1;
		if(!_floppy->_readbit(bit))
			_regs[DMAREG] |= bit;
		if (_bit == 15 && (_regs[ADKCON] & 0x400)==0 && !(_regs[DSKBYT] & 0x2000) && (_regs[DSKLEN] & 0x3fff) >= 0) {
			if ((_regs[DSKLEN] & 0x3fff) > 0) {
				//_regs[DMAVAL] = SL((u8)_regs[DMAREG],8);
				//_regs[DMAVAL] |= (u8)_regs[DMAREG];
				_regs[DMAVAL] = _regs[DMAREG];
				_write();
			}
		}
		_bit++;
	}

X:
	if((_floppy->_cycles[1]/_floppy->_speed) >=_floppy->_len){//index
		_ciab->Trigger(IDEVICE_TRIGGER_IRQ,FLOPPY,1,(void *)(u64)0x10);
		_floppy->_cycles[1]-=_floppy->_speed;
	}
Z:
	_cycles=__cycles;
	return 0;
}

int amiga500dev::__fdc::write(u32 a,u16 v){
	u32 res;

	res=0x10000;
	switch((u8)SR(a,1)){
		case REG_DSKSYNC:
			_regs[DSKSYNC]=v;
			break;
		case REG_DMACON:{
			res=(int)(s16)res;
			_regs[DMACON]=v;
			_dmaen=0;
			if((v & (DMACON_DSKEN|DMACON_DMAEN)) == (DMACON_DSKEN|DMACON_DMAEN))
				_dmaen=1;
			if(_floppy)
				_floppy->_changed=1;
			res=0;
			goto A;
		}
		break;
		case REG_DSKLEN:
			if(_floppy)
				_floppy->_changed=1;
			//printf("REG_DSKLEN %x:%x %x %x\n",_regs[DSKLEN],v,_dmaen,_state);
		//	if(_floppy && _floppy->_cyl==74 && v==0x4000 && _state) EnterDebugMode();
			if(/*_state == 0 &&*/ (v & 0x8000) && (_regs[DSKLEN] & 0x8000)){
				_start();
				//DLOG("FDC W %x %x",a,v);
			}
			_regs[DSKLEN]=v;
			goto A;
		break;
		case REG_ADKCON:
			_regs[ADKCON]=v;
			_sync=SR(v,10);
		//	printf("REG_ADKCON %x %x\n",_regs[ADKCON],_dmaen);
			if(_floppy)
				_floppy->_changed=1;
			goto A;
		break;
		case REG_DSKPTH:
		case REG_DSKPTL:
			if(_floppy)
				_floppy->_changed=1;
			res|=1;
			goto Z;
		default:
			printf("fw %x %x ",a,v);
		break;
	}

	goto Z;
A:
	_regs[DSKBYT] &= ~0x6000;
	_on = _dmaen && !_sync;
Z:
	//if(SR(res,16)) DLOG("FDC W %x %x",a,v);
	return (int)(s16)res;
}

int amiga500dev::__fdc::read(u32 a,u16 *p){
	switch(SR((u8)a,1)){
		case REG_DSKBYTR:
			printf("%s %x\n",__FUNCTION__,a);
		break;
	}
	return 1;
}

int amiga500dev::__fdc::Trigger(u32 what,u32 devId,u32 portId,void *v){
	u8 b =(u8)(u64)v;
	switch(what){
		case IDEVICE_TRIGGER_PORT_WRITE:
			_side=SR(b ^ 4 ,2);
			_dir=SR(b,1);
			_step_pulse=b;
			DLOG("FDC %x %x %u D%u S%u M%d P%d",b,SR(b,3)&15,SR(b^0x80,7),_dir,_side,_on,_step_pulse != _step && _step_pulse);
			_ciaareg[__cia::PRA] |= 0x3c;

			_floppy=NULL;
			for(int i=0;i<sizeof(_floppies)/sizeof(__floppy);i++){
				if(_floppies[i]._side!=_side){
					_floppies[i]._side=_side;
					_floppies[i]._refill=1;
				}
				if(!BVT(b,3+i)){
					//if(BVT(_selected,i) && _floppies[i]._cstep==0)
					//	_floppies[i]._on=SR(b ^ 0x80,7) | _on;
					/*if(_floppies[i]._cyl==0)
						_ciaareg[__cia::PRA] &= ~(0x10);
					if(_floppies[i]._eject)//00C194A8
						_ciaareg[__cia::PRA] &= ~4;
					//if(_floppies[i]._eject)
						_ciaareg[__cia::PRA] &= ~8;//wite
					//if(!_floppies[i]._eject || !_floppies[i]._on)
					//	_ciaareg[__cia::PRA] &= ~(0x20);
					//if(!_floppies[i]._eject || !_floppies[i]._on)
					//	_ciaareg[__cia::PRA] &= ~(0x20);
					*/
					if(!_floppy){
						_floppy=&_floppies[i];
						_floppy->_dir=_dir;
						//_floppy->_side=_side;
						_floppy->_selected=1;
						if(_step_pulse != _step && _step_pulse)
							_floppy->_step(b);
						_floppy->_changed = 1;
					}
				}
				else
					_floppies[i]._selected=0;
			}

			_step=_step_pulse;
			_selected = SR(b,3);
		//	_on=SR(b ^ 0x80,7);
			//_regs[9]=b;

			if(_floppy){
				//if(BVT(_selected,i) && _floppies[i]._cstep==0)
						_floppy->_on=SR(b ^ 0x80,7) | _on;
				if(!_floppy->_on){
					_floppy->_cycles[1]=0;
				}
			//	else if(!_floppy->_eject)
			//		_ciaareg[__cia::PRA] &= ~(0x20);
				if(!_floppy->_cyl)
					_ciaareg[__cia::PRA] &= ~(0x10);
				if(!_floppy->_empty/* && _floppy->_on*/)
					_ciaareg[__cia::PRA] &= ~(0x20|8);
				if(_floppy->_empty)
					_ciaareg[__cia::PRA] &= ~0x4;
				//if(!(_ciaareg[__cia::PRA]&0x3c)) EnterDebugMode();
			}
			else{
				//_abort();
			}
		break;
	}
	return 0;
}

int amiga500dev::__fdc::_add(char *c,int idx){
	if(!c || !c[0])
		return -1;
	return _floppies[idx]._add(c);
}

int amiga500dev::__fdc::_start(){
	_state=1;
	//_regs[DMAVAL]=_regs[DMAREG]=0;
	//_bit=0;
	_cycles=__cycles;
	for(int i=0;i<sizeof(_floppies)/sizeof(__floppy);i++){
		__floppy *p=&_floppies[i];
		p->_cycles[0]=0;
		p->_cycles[1]=0;
		//if(p->_mfmpos < 5000) p->_mfmpos=27244;
	}
	DLOG("FDC READ P:%u M:%u T:%d PT;%08X L:%04X",_floppy ? _floppy->_mfmpos : 0,_floppy ? _floppy->_on : 0,
		_floppy ? _floppy->_cyl*2+_floppy->_side : -1,BELE32(ACHIPREG32_(_ioreg,REG_DSKPTH)),_regs[DSKLEN]);
	EnterDebugMode(DEBUG_BREAK_DEVICE);
	return 0;
}

int amiga500dev::__fdc::_end(){
	if(_regs[DSKBYT] & 0x2000){
		_regs[DSKBYT] &= ~0x2000;
	}
	//printf("fdc end %x %x %u\n",_state,_regs[DSKPT],_floppy ? _floppy->_mfmpos : 0);
	//_ciab->Trigger(IDEVICE_TRIGGER_IRQ,FLOPPY,0,(void *)(u64)_log2(INTENA_DSKBLK));
	if(_state){
		DLOG("FDC IRQ DSKBLK %u",_floppy->_cyl*2+_floppy->_side);
	//	printf("FDC IRQ DSKBLK %u\n",_floppy->_cyl*2+_floppy->_side);

		machine->OnEvent((u8)(u64)_log2(INTENA_DSKBLK),1);
		//if(_floppy->_cyl*2+_floppy->_side == 1) EnterDebugMode();
	}
	_state=0;
	if(_sync) _on=0;
	return 0;
}

int amiga500dev::__fdc::_abort(){
	if(_state) printf("fdc abort\n");
	_state=0;
	//_dma_state=0;
	return 0;
}

int amiga500dev::__fdc::_write(){
	//printf("wm %x\n",_regs[DSKPT]);
	void *p;
	u16 v;

	v=_regs[DMAVAL];
	if(_sync && !_on){
		if(v==_regs[DSKSYNC]){
			_bit=15;
			_on=1;
			machine->OnEvent((u8)(u64)_log2(INTENA_DSKSYN),1);
			return 0;
		}
		return 1;
	}
	if(!_on)
		return 1;
	_regs[DSKLEN]--;
	switch(SR(*(u32 *)&_regs[DSKPT],20)){
		case 0xf:
			p=(u8 *)&_mem[MI_BIOS];
		break;
		case 0xc:
			p=(u8 *)&_mem[MI_RAM2];
		break;
		default:
			p=(u8 *)_mem;
		break;
	}
	p = (u8 *)p + *(u32 *)&_regs[DSKPT];
	*(u32 *)&_regs[DSKPT] += 2;
	*((u16 *)p) = SWAP16(v);
	if(!(_regs[DSKLEN] & 0x3fff))
		_end();
	return 0;
}

amiga500dev::__fdc::__floppy::__floppy() : vector<trackid>(){
	_streamer=NULL;
	_fn="";
	_buf=NULL;
	_status=0;
	_cyls=80;
	_turbo=1;
	_index=0;
}

amiga500dev::__fdc::__floppy::~__floppy(){
	_close();
}

int amiga500dev::__fdc::__floppy::_step(int s){
	u32 cyl;

	if(_cycles[2]){
		DLOG("FDC step ignored %u",_cycles[2]);
		return 0;
	}

	//printf("step CY:%d D:%d Si:%d %u\n",_cyl,_dir,_side,_mfmpos);
	cyl=_cyl;
	if(!_dir){
		if(_cyl < _ntracks)
			_cyl++;
	}
	else if(_cyl)
		_cyl--;
	if(_cyl != cyl && !_empty){
		_refill=1;
		//_mfmpos=0;
	}
	_cycles[2]=45*379;//0x2e scanline
	return 0;
}

int amiga500dev::__fdc::__floppy::_decode(u32 tr,void *p){
	u32 len,dstmfmoffset;

	_track=&_trackdata[tr];
	dstmfmoffset=0;
	if(_track->type==TRACK_AMIGADOS){
		len = _nsecs * 544 + FLOPPY_GAP_LEN;
		dstmfmoffset += FLOPPY_GAP_LEN;
		_len=_track->len=len*2*8;
		_skip=(FLOPPY_GAP_LEN * 8) / 3 * 2;
	//	printf(" ADOS T:%u %u %u O:%u-%u %u",tr,len,dstmfmoffset,_track->offs,_track->len,_nsecs);
		memset (p, 0xaa, len * 2);
		for (u32 sec = 0; sec < _nsecs; sec++) {
			u8 secbuf[544];
			u16 mfmbuf[544];
			int i;
			u32 deven, dodd,hck = 0, dck = 0;

			secbuf[0] = secbuf[1] = 0x00;
			secbuf[2] = secbuf[3] = 0xa1;
			secbuf[4] = 0xff;
			secbuf[5] = tr;
			secbuf[6] = sec;
			secbuf[7] = _nsecs - sec;

			for (i = 8; i < 24; i++)
				secbuf[i] = 0;

			_streamer->Seek(_track->offs+sec*512,SEEK_SET);//35 197920
			_streamer->Read(&secbuf[32],512,(u32 *)&i);

			//printf("read %u %u ",_track->offs+sec*512,i);

			mfmbuf[0] = mfmbuf[1] = 0xaaaa;
			mfmbuf[2] = mfmbuf[3] = 0x4489;

			deven = ((secbuf[4] << 24) | (secbuf[5] << 16) | (secbuf[6] << 8) | (secbuf[7]));
			dodd = deven >> 1;
			deven &= 0x55555555;
			dodd &= 0x55555555;

			mfmbuf[4] = dodd >> 16;
			mfmbuf[5] = dodd;
			mfmbuf[6] = deven >> 16;
			mfmbuf[7] = deven;

			for (i = 8; i < 48; i++)
				mfmbuf[i] = 0xaaaa;
			for (i = 0; i < 512; i += 4) {
				deven = ((secbuf[i + 32] << 24) | (secbuf[i + 33] << 16) | (secbuf[i + 34] << 8) | (secbuf[i + 35]));
				dodd = deven >> 1;
				deven &= 0x55555555;
				dodd &= 0x55555555;
				mfmbuf[(i >> 1) + 32] = dodd >> 16;
				mfmbuf[(i >> 1) + 33] = dodd;
				mfmbuf[(i >> 1) + 256 + 32] = deven >> 16;
				mfmbuf[(i >> 1) + 256 + 33] = deven;
			}

			for (i = 4; i < 24; i += 2)
				hck ^= (mfmbuf[i] << 16) | mfmbuf[i + 1];

			deven = dodd = hck;
			dodd >>= 1;
			mfmbuf[24] = dodd >> 16;
			mfmbuf[25] = dodd;
			mfmbuf[26] = deven >> 16;
			mfmbuf[27] = deven;

			for (i = 32; i < 544; i += 2)
				dck ^= (mfmbuf[i] << 16) | mfmbuf[i + 1];

			deven = dodd = dck;
			dodd >>= 1;
			mfmbuf[28] = dodd >> 16;
			mfmbuf[29] = dodd;
			mfmbuf[30] = deven >> 16;
			mfmbuf[31] = deven;
			//mfmcode (mfmbuf + 4, 544 - 4);

			u32 lastword = 0;
			for (i=4;i<544;i++) {
				u32 v = mfmbuf[i];
				u32 lv = (lastword << 16) | v;
				u32 nlv = 0x55555555 & ~lv;
				u32 mfmbits = (nlv << 1) & (nlv >> 1);
				mfmbuf[i] = v | mfmbits;
				lastword = v;
			}

			for (i = 0; i < 544; i++) {
				((u16 *)p)[dstmfmoffset % len] = mfmbuf[i];
				dstmfmoffset++;
			}
#ifdef _DEVELOP
			if(tr==35){
				sprintf((char *)secbuf,"mfmbuf%d.raw",tr);
				FILE *fp=fopen((char *)secbuf,"ab+");
				fwrite(mfmbuf,2,544,fp);
				fclose(fp);
			}
#endif
		}
		_speed=NORMAL_FLOPPY_SPEED * _track->len / (2 * 8 * FLOPPY_WRITE_LEN * (_ddhd+1));
		//printf(" D:%u %u %u V:%x\n",dstmfmoffset,_speed,_len,((u16 *)p)[0]);
	}
	return 0;
}

int amiga500dev::__fdc::__floppy::_readbit(int &v){
	if(!_streamer || !_buf || !_on)
		return -1;
	//printf("eaib %u\n",_mfmpos);
    v = (_buf[_mfmpos >> 4] & SL(1 , 15 - (_mfmpos & 15))) ? 1 : 0;
    _mfmpos=(_mfmpos+1) % _len;
	return 0;
}

int amiga500dev::__fdc::__floppy::_writebit(int &){
	if(!_streamer)
		return -1;
	return 0;
}

int amiga500dev::__fdc::__floppy::_reset(){
	_fn="";
	_size=0;
	_ntracks=0;
	_nsecs=0;
	_track=NULL;
	memset(_cycles,0,sizeof(_cycles));
	_cyl=0;//c1ca6c
	_status=0;
	_mfmpos=0;
	_ddhd=0;
	_skip=0;
	_speed=NORMAL_FLOPPY_SPEED;
	_len=2 * 8 * FLOPPY_WRITE_LEN * (_ddhd+1);
	_empty=1;
	_wp=1;
	_eject=1;
	_refill=1;
	clear();
	return 0;
}

int amiga500dev::__fdc::__floppy::_close(){
	if(_streamer){
		_streamer->Close();
		delete _streamer;
		_streamer=NULL;
	}
	if(_buf)
		delete []_buf;
	_buf=NULL;
	return _reset();
}

int amiga500dev::__fdc::__floppy::_add(char *c){
	_close();
	if(!(_streamer=new FileStream()) || _streamer->Open(c))
		return -1;
	_fn=c;
	printf("open %s %x ",c,0);
	_streamer->Seek(0,SEEK_END);
	_streamer->Tell(&_size);
	printf("%u ",(u32)_size);
	_streamer->Seek(0,SEEK_SET);
	_ddhd=0;
	if (_size == 720 * 1024 || _size == 1440 * 1024){
	}
	else{
		_filetype=ADF_NORMAL;
		if (_size >= 160 * 22 * 512) {
			_ntracks = _size / (512 * (_nsecs = 22));
			_ddhd = 1;
		}
		else
			_ntracks = _size / (512 * (_nsecs = 11));
		printf("T:%u S:%u %u ",_ntracks,_nsecs,_ddhd);
		for (int i = 0; i < _ntracks; i++) {
			trackid *tid = &_trackdata[i];
			tid->type = TRACK_AMIGADOS;
			tid->elen = 512 * _nsecs;
			tid->bitlen = 0;
			tid->offs = i * 512 * _nsecs;
			tid->len = 0;
			push_back({(u16)(512 * _nsecs),i * 512 * _nsecs,TRACK_AMIGADOS});
		}
		_eject=0;
		_empty=0;
	}
	if(!_buf && !(_buf=new u16[0x4000]))
		return -2;
	_refill=1;
	printf("\n");
	return 0;
}

int amiga500dev::__potgo::reset(){
	_cycles=__cycles;
	_status=0;
	memset(_regs,0,sizeof(_regs));
	_regs[GOR]=0x500;
	ACHIPREG_(_ioreg,REG_POTGOR) = SWAP16(_regs[GOR]);
	return 0;
}

int amiga500dev::__potgo::update(int cyc){
	u32 d=__cycles >= _cycles ? __cycles-_cycles: _freq-_cycles + __cycles;
	if(d < 15) goto Z;
	//_cycles=__cycles;
Z:
	return 0;
}

int amiga500dev::__potgo::Init(int,void *a,void *b,u32 f){
	_ioreg=(u16 *)b;
	_mem=(u8 *)a;
	_freq=f;
	return 0;
}

int amiga500dev::__potgo:: read(u32 a,u16 *p){
	//DLOG("POTGO R %x",a);
	switch((u8)SR(a,1)){
		case REG_POTGOR:{
			for (int i = 0; i < 2; i++) {
				u16 p9dir = 0x0800 << (i * 4); /* output enable P9 */
				u16 p9dat = 0x0400 << (i * 4); /* data P9 */
				u16 p5dir = 0x0200 << (i * 4); /* output enable P5 */
				u16 p5dat = 0x0100 << (i * 4); /* data P5 */

				if (!i) {
					if (!(_regs[GO] & p5dir))
						_regs[GOR] |= p5dat;
					if (!(_regs[GO] & p9dir))
						_regs[GOR] |= p9dat;
				}
			}
			*p=SWAP16(_regs[GOR] & 0x5500);
		}
			return 1;
		default:
			printf("%s %x\n",__FUNCTION__,a);
		break;
	}
	return 0;
}

int amiga500dev::__potgo:: write(u32 a,u16 v){
	//DLOG("POTGO W %x %x",a,v);
	switch((u8)SR(a,1)){
		case REG_POTGO:
		//printf("%s %x %x %x\n",__FUNCTION__,a,v,_regs[GO]);
			_regs[GO]=(_regs[GO]&0x5500) | (v&0xaa00);
			for (int i = 0; i < 8; i += 2) {
				if (v & SL(0x200,i)) {
					u16 data = 0x0100 << i;
					_regs[GO] &= ~data;
					_regs[GO] |= v & data;
				}
			}
			if(v&1){
				_regs[POT0]=_regs[POT1]=0;
				_cycles=0;
			}
			return 0;
		default:
			printf("%s %x->%x\n",__FUNCTION__,a,v);
		break;
	}
	return 0;
}

int amiga500dev::__keyboard::reset(){
	_cycles=__cycles;
	_status=0;
	return 0;
}

int amiga500dev::__keyboard::update(int cyc){
	u32 d=__cycles >= _cycles ? __cycles-_cycles: _freq-_cycles + __cycles;
	if(!(d=SR(d,16))) return 1;
	_cycles=__cycles;
	_key=(u8)0x4;
	if(!size())
		return 1;
	switch(_unread){
		case 2:
			_unread=0;
			_wait=0;
		break;
		case 0:
			switch(_init){
				case 2:{
					EVENTMSG &m=front();
					erase(begin());
				//	printf("%x:",m._buf[1]);
					_translate(m._buf[1],0);
				//	printf("%x:",m._buf[1]);
					u8 key=m._buf[1];
				//	key=0x44;
					key |= m._buf[0] ? 0x80 : 0;
				//	printf("%x:",key);
					_key=~((key<<1)|(key>>7));
				//	printf("%x\n",_key);
					_unread=1;
					_wait=0;
				}
				break;
				default:
					_init++;
				break;
			}
			_ciaa->Trigger(IDEVICE_TRIGGER_IRQ,KEYBOARD,0,(void *)8);
		break;
		default:
			if(++_wait == 15)
				_unread=0;
		break;
	}
	return 0;
}

int amiga500dev::__keyboard::Init(int,void *,void *,u32 f){
	_freq=f;
	_ciaa=(IDevice *)CIAA;
	cpu->Query(ICORE_QUERY_IO_DEVICE,&_ciaa);
	_ciaareg=(u8 *)CIAA;
	cpu->Query(ICORE_QUERY_IO_PORT,&_ciaareg);
	return 0;
}

int amiga500dev::__keyboard::_translate(u32 &key,u32){
	printf("key %x\n",key);
	switch(toupper(key)){
		case GDK_KEY_F1:
		case GDK_KEY_F2:
		case GDK_KEY_F3:
		case GDK_KEY_F4:
		case GDK_KEY_F5:
			key=0x50 + (key-GDK_KEY_F1);
			return 0;
		case 'A':
			key=0x20;
			return 0;
		case 'B':
			key=0x35;
			return 0;
		case 'C':
			key=0x33;
			return 0;
		case 'D':
			key=0x22;
			return 0;
		case 0x20:
			return 0;
		default:
		case 0xd:
			key=0x44;
			return  0;
	}
	return -1;
}

int amiga500dev::__keyboard::read(u8 *p){
	if(_unread==1)
		_unread=2;
	*p=_key;
	/*{
		u32 r;

		cpu->Query(ICORE_QUERY_PC,&r);
		printf("__keyboard::%s PC:%x\n",__FUNCTION__,r);
	}*/
	return 0;
}

int amiga500dev::__rs232::reset(){
	_status=0;
	_cycles=__cycles;
	if(_ioreg){
		//ACHIPREG(REG_SERDATR)=SWAP16(SERDATR_RXD | SERDATR_TSRE | SERDATR_TBE);
		//_regs[SDR]=SR(_ioreg[REG_SERDATR],8);
	}
	return 0;
}

int amiga500dev::__rs232::update(int cyc){
	return 0;
}

int amiga500dev::__rs232::Init(int,void *a,void *b,u32 f){
	ADevice::Init(0,a,b,f);
	return 0;
}

int amiga500dev::__rs232::write(u32 a,u16 v){
	switch((u8)SR(a,1)){
		case REG_SERDAT:{
			u16 vv=SWAP16(ACHIPREG(REG_SERDATR));
			printf("SERDAT %x %x\n",vv,v);
			if(vv & SERDATR_TSRE){
				_tx_shift=v;
				v=0;
				vv = (vv & ~SERDATR_TSRE)|SERDATR_TBE;
				//vv |= SERDATR_TBE;
				//machine->OnEvent(0,0);
			}
			else{
				vv =(vv&~SERDATR_TSRE)|SERDATR_RBF;
			}
			ACHIPREG(REG_SERDAT)=SWAP16(vv);
			ACHIPREG(REG_SERDATR)=SWAP16(vv);
			return 1;
		}
		case REG_SERPER:{
			u32 r;

			cpu->Query(ICORE_QUERY_PC,&r);
		printf("REG_SERPER %x PC:%x\n",v,r);
	}
		break;
	}
	return 0;
}

int amiga500dev::__rs232::read(u32,u16 *){
	return 0;
}

int amiga500dev::__mouse::reset(){
	_status=0;
	memset(_buf,0,sizeof(_buf));
	_cycles=__cycles;
	return 0;
}

int amiga500dev::__mouse::update(int cyc){
	u32 d,bit;

	d=(__cycles >= _cycles ? __cycles - _cycles : (_freq-_cycles) + __cycles);
	if(!SR(d,4))
		goto Z;
	_cycles=__cycles;
	if(size()){
		EVENTMSG &m=front();
		erase(begin());
		memcpy(_buf,m._buf,10*sizeof(u32));
	}
Z:
	bit=SL(0x40,_idx);
	if((_ciaareg[__cia::DDRA] & bit)==0){
		if(!(_buf[2] & 1))
			_ciaareg[__cia::PRA] |= bit;
		else
			_ciaareg[__cia::PRA] &= ~bit;
		//printf("%d %x\n",_idx,_ciaareg[__cia::PRA]);
	}
	return 0;
}

int amiga500dev::__mouse::Init(int i,void *a,void *b,u32 f){
	if(ADevice::Init(i,a,b,f))
		return -1;
	_ciaa=(IDevice *)CIAA;
	cpu->Query(ICORE_QUERY_IO_DEVICE,&_ciaa);
	_ciaareg=(u8 *)CIAA;
	cpu->Query(ICORE_QUERY_IO_PORT,&_ciaareg);
	return 0;
}

int amiga500dev::__mouse::write(u32 a,u16 v){
	return 0;
}

int amiga500dev::__mouse::read(u32 a,u16 *p){
	switch(SR((u8)a,1)){
		default:{
			u16 l,r,t,b,v;

			v = _buf[0]|SL(_buf[1],8);
			v &= ~0x0303;
			b=SR(_buf[3],3)&1;
			t=SR(_buf[3],2)&1;
			r=SR(_buf[3],1)&1;
			l=_buf[3]&1;
			if(l) t=!t;
			if(r) b=!b;
			v|=b;//top
			v|=SL(r,1);//right
			v|=SL(t,8);//left
			v|=SL(l,9);//bottom
			*p=SWAP16(v);
		}
		break;
	}
	return 0;
}

s32 amiga500dev::fn_read_io(u32 a,void *pmem,void *pdata,u32){
	u8 reg;

	switch(reg=(u8)SR(a,1)){
		case REG_DMACON:
			*(u16 *)pdata = 0x1fff|(*(u16 *)pdata & (DMACON_BBUSY|DMACON_BZERO));
			return 1;
		case REG_JOY0DAT:
		case REG_JOY1DAT:
			_joy[reg-REG_JOY0DAT].read(a,(u16 *)pdata);
		break;
		case REG_POT0DAT:
		case REG_POT1DAT:
			printf("%s %x\n",__FUNCTION__,a);
		break;
		case REG_POTGOR:
			return _potgo.read(a,(u16 *)pdata);
		case REG_DSKBYTR:
			return _fdc.read(a,(u16 *)pdata);
		case REG_CLXDAT:
			printf("%s %x\n",__FUNCTION__,a);
			*(u16 *)pdata = *(u16 *)pmem;
			*(u16 *)pmem=0;
			Denise::write(REG_CLXDAT*2,0);
			break;
	}
	return 0;
}

s32 amiga500dev::fn_write_io(u32 a,void *pmem,void *pdata,u32 attr){
	u8 reg;

	switch(reg=(u8)SR(a,1)){
		case REG_BLTDDAT:   case REG_VPOSR:     case REG_VHPOSR:	case REG_DMACONR:
		case REG_DSKDATR:   case REG_JOY0DAT:   case REG_JOY1DAT:   case REG_CLXDAT:
		case REG_ADKCONR:   case REG_POT0DAT:   case REG_POT1DAT:   case REG_POTGOR:
		case REG_SERDATR:   case REG_DSKBYTR:   case REG_INTENAR:   case REG_INTREQR:
			return 0;
		case REG_DMACON:{
			u16 r,v=SWAP16(*(u16 *)pdata);

			r=SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_DMACON));
			v &= ~(DMACON_BZERO|DMACON_BBUSY);
			REG_SETCLR(r,v);
			ACHIPREG_(M68000Cpu::_ioreg,REG_DMACON)=SWAP16(r);
			ACHIPREG_(M68000Cpu::_ioreg,REG_DMACONR)=ACHIPREG_(M68000Cpu::_ioreg,REG_DMACON);
			_agnus.write(a,r);
			_fdc.write(a,r);
			//printf("REG_DMACON %x %x PC %x\n",*(u16 *)pdata,r,_pc);
			Paula::write(a,r);
			return Denise::write(a,r);
		}
		case REG_INTENA:{
			u16 r,v=SWAP16(*(u16 *)pdata);

			r=SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_INTENA));
			REG_SETCLR(r,v);
			ACHIPREG_(M68000Cpu::_ioreg,REG_INTENA)=SWAP16(r);
			ACHIPREG_(M68000Cpu::_ioreg,REG_INTENAR)=ACHIPREG_(M68000Cpu::_ioreg,REG_INTENA);
			//printf("REG_INTENA %x  %x %x\n",ACHIPREG_(M68000Cpu::_ioreg,REG_INTENA),v,_pc);
		//	if(_irq_pending) machine->OnEvent(0,(LPVOID)-1);
			if(r&0x4000) _setStatus(S_IRQ_CHECK);
			return 0;
		}
		case REG_INTREQ:{
			u16 r,v=SWAP16(*(u16 *)pdata);
			r=SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQ));
			REG_SETCLR(r,v);
			ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQ)=SWAP16(r);
			ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQR)=SWAP16(r);
			REG_SETCLR(_irq_pending,v);
			if(_irq_pending) _setStatus(S_IRQ_CHECK);
			return 0;
		}
		break;
		case REG_ADKCON:{
			u16 r,v = SWAP16(*(u16 *)pdata);
			r=SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_ADKCON));
			REG_SETCLR(r,v);
			ACHIPREG_(M68000Cpu::_ioreg,REG_ADKCON)=SWAP16(r);
			ACHIPREG_(M68000Cpu::_ioreg,REG_ADKCONR)=SWAP16(r);
			_fdc.write(a,r);
			Paula::write(a,r);
			return 0;
		}
		case REG_SPR0POS:	case REG_SPR1POS:	case REG_SPR2POS:	case REG_SPR3POS:
		case REG_SPR4POS:	case REG_SPR5POS:	case REG_SPR6POS:	case REG_SPR7POS:
		case REG_DDFSTOP:	case REG_VPOSW:
		case REG_SPR0CTL:   case REG_SPR1CTL:   case REG_SPR2CTL:   case REG_SPR3CTL:
		case REG_SPR4CTL:   case REG_SPR5CTL:   case REG_SPR6CTL:   case REG_SPR7CTL:
		case REG_SPR0PTH:   case REG_SPR1PTH:   case REG_SPR2PTH:   case REG_SPR3PTH:
		case REG_SPR4PTH:   case REG_SPR5PTH:   case REG_SPR6PTH:   case REG_SPR7PTH:
		case REG_SPR0PTL:   case REG_SPR1PTL:   case REG_SPR2PTL:   case REG_SPR3PTL:
		case REG_SPR4PTL:   case REG_SPR5PTL:   case REG_SPR6PTL:   case REG_SPR7PTL:
		case REG_SPR0DATA:  case REG_SPR1DATA:  case REG_SPR2DATA:  case REG_SPR3DATA:
		case REG_SPR4DATA:  case REG_SPR5DATA:  case REG_SPR6DATA:  case REG_SPR7DATA:
		case REG_BPLCON0:	case REG_BPLCON1:	case REG_BPLCON2:
		case REG_BPL1PTH:	case REG_BPL2PTH:	case REG_BPL3PTH:	case REG_BPL4PTH:
		case REG_BPL5PTH:	case REG_BPL6PTH:
		case REG_DIWSTRT:	case REG_DIWSTOP:	case REG_DDFSTRT:
		case REG_COLOR00:	case REG_COLOR01:	case REG_COLOR02:	case REG_COLOR03:
		case REG_COLOR04:	case REG_COLOR05:	case REG_COLOR06:	case REG_COLOR07:
		case REG_COLOR08:	case REG_COLOR09:	case REG_COLOR10:	case REG_COLOR11:
		case REG_COLOR12:	case REG_COLOR13:	case REG_COLOR14:	case REG_COLOR15:
		case REG_COLOR16:	case REG_COLOR17:	case REG_COLOR18:	case REG_COLOR19:
		case REG_COLOR20:	case REG_COLOR21:	case REG_COLOR22:	case REG_COLOR23:
		case REG_COLOR24:	case REG_COLOR25:	case REG_COLOR26:	case REG_COLOR27:
		case REG_COLOR28:	case REG_COLOR29:	case REG_COLOR30:	case REG_COLOR31:
		case REG_CLXCON:	case REG_BPL1MOD:	case REG_BPL2MOD:
		{
			u32 v=*(u32 *)pdata;
			//ACHIPREG_(M68000Cpu::_ioreg,reg)=*(u16 *)pdata;
			if(attr & AM_DWORD){
			//	ACHIPREG_(M68000Cpu::_ioreg,reg+1)=SR(*(u32 *)pdata,16);
				Denise::write(a+2,(SR(v,16)));
				//fn_write_io(a+2,0,(u8 *)pdata + 2,AM_WRITE|AM_WORD);
				v=((u16)v);
			}
			return Denise::write(a,(u16)v);
		}
		case REG_SERPER:
		case REG_SERDAT:
		//EnterDebugMode();
			return _rs232.write(a,SWAP16(*(u16 *)pdata));
		case REG_BLTCON0:
		case REG_COP1LCH:
		case REG_COP1LCL:
		case REG_COP2LCH:
		case REG_COP2LCL:
		case REG_COPJMP1:
		case REG_COPJMP2:
		case REG_COPCON:
		case REG_BLTCON1:
		case REG_BLTSIZE:
			return _agnus.write(a,SWAP16(*(u16 *)pdata));
		break;
		case REG_DSKSYNC:
		case REG_DSKPTH:
		case REG_DSKPTL:
		case REG_DSKLEN:
		case REG_DSKDAT:
			return _fdc.write(a,SWAP16(*(u16 *)pdata));
		case REG_BLTAPTL:
		case REG_BLTBPTL:
		case REG_BLTCPTL:
		case REG_BLTDPTL:
			//*(u16 *)pdata &= ~0x8000;
		break;
		case REG_POTGO:
			return _potgo.write(a,SWAP16(*(u16 *)pdata));
		case REG_JOYTEST:
		//printf("reg %x\n",a);
		break;
		case REG_AUD0LCH:
        case REG_AUD0LCL:
        case REG_AUD0LEN:
        case REG_AUD0PER:
        case REG_AUD0VOL:
        case REG_AUD0DAT:
        case REG_AUD1LCH:
        case REG_AUD1LCL:
        case REG_AUD1LEN:
        case REG_AUD1PER:
        case REG_AUD1VOL:
        case REG_AUD1DAT:
        case REG_AUD2LCH:
        case REG_AUD2LCL:
        case REG_AUD2LEN:
        case REG_AUD2PER:
        case REG_AUD2VOL:
        case REG_AUD2DAT:
        case REG_AUD3LCH:
        case REG_AUD3LCL:
        case REG_AUD3LEN:
        case REG_AUD3PER:
        case REG_AUD3VOL:
        case REG_AUD3DAT:{
			u32 v=*(u32 *)pdata;
			//ACHIPREG_(M68000Cpu::_ioreg,reg)=*(u16 *)pdata;
			if(attr & AM_DWORD){
			//	ACHIPREG_(M68000Cpu::_ioreg,reg+1)=SR(*(u32 *)pdata,16);
				Paula::write(a+2,SR(v,16));
				v=(u16)v;
			}
			return Paula::write(a,v);
		}
		default:
		//	printf("reg %x\n",a);
			break;
	}
	return 1;
}


};