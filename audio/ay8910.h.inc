struct __ay8910{
	enum{
		AY_AFINE    = 0x00,
		AY_ACOARSE  = 0x01,
		AY_BFINE    = 0x02,
		AY_BCOARSE  = 0x03,
		AY_CFINE    = 0x04,
		AY_CCOARSE  = 0x05,
		AY_NOISEPER = 0x06,
		AY_ENABLE   = 0x07,
		AY_AVOL     = 0x08,
		AY_BVOL     = 0x09,
		AY_CVOL     = 0x0a,
		AY_EAFINE   = 0x0b,
		AY_EACOARSE = 0x0c,
		AY_EASHAPE  = 0x0d,
		AY_PORTA    = 0x0e,
		AY_PORTB    = 0x0f,
		AY_EBFINE   = 0x10,
		AY_EBCOARSE = 0x11,
		AY_ECFINE   = 0x12,
		AY_ECCOARSE = 0x13,
		AY_EBSHAPE  = 0x14,
		AY_ECSHAPE  = 0x15,
		AY_ADUTY    = 0x16,
		AY_BDUTY    = 0x17,
		AY_CDUTY    = 0x18,
		AY_NOISEAND = 0x19,
		AY_NOISEOR  = 0x1a,
		AY_TEST     = 0x1f
	};
	struct{
		u32 _count,_period,_number;
		u8 _prescale;
	} _noise;

	union{
		struct{
			unsigned int _idx:2;
			unsigned int _enabled:1;
			unsigned int _reg:5;
			unsigned int _active:1;
		};
		u32 _value;
	};

	struct __tone{
		union{
			struct{
				unsigned int _idx:2;
				unsigned int _channel:2;
				unsigned int _invalidate:1;
				unsigned int _noise_enable:1;
				unsigned int _env:1;
				unsigned int _vol:4;
				unsigned int _enabled:1;
			};
			u32 _status;
		};

		struct{
			u32 _period,_count,_vol;
			u8 _holding,_attack,_hold;
			s8 _step;
		} _envelope;

		u32 _period,_count;
		u8 _duty;

		u8 *_regs;

		int output(int,struct __ay8910 &);
		int reset();
		int init(int,int,struct __ay8910 &);
	} _channels[3];

	u8 _regs[32];

	static const int _freq=KHZ(96);

	int reset();
	int write(u8);
	int read(u8 *);
	int output(int);
	int init(int);
};