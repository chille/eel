//keyword: "break default func interface select case map struct chan else goto package switch const fallthrough range type continue for import var go defer bool byte complex64 complex128 float32 float64 int8 int16 int32 int64 string uint8 uint16 uint32 uint64 int uint uintptr rune",
//built_in: "append cap close complex copy imag len make new panic print println real recover delete"};


hljs.registerLanguage("eel", function(e){
	var EEL_KEYWORDS = {
		keyword: "function procedure if local return throw upvalue export",
		literal: "true false",
//		built_in: "print"
	};

	return {
		k: EEL_KEYWORDS,
		i: "</",
		c:
		[
			// Comment
			e.CLCM,

//			e.CBCM,

			// String
			{
				cN: "string",
				v:
				[
					e.QSM,
/*
					{
						cN: "lol",
						b:"'",
						e:"[^\\\\]'"
					}
*/
				]
			},

			// Number
			{
				cN:"number",
				v:
				[
					{
						b:e.CNR+"[dflsi]",
						r:1
					},
					e.CNM
				]
			},

			// =
/*
			{
				b:/:=/
			},
*/
/*
			{
				cN: 'export',
				bK: 'export',
				e: /[{;:]/,
				c:
				[
					{
						b: /</,
						e: />/,
						c: ['self']
					}, // skip generic stuff
					e.TM
				]
			},
*/
			{
				cN: "function",
				bK: "function",
				e: /\s*\{/,
				eE: true,

				c:
				[
					e.TM,
					{
						cN: "params",
						b: /\(/,
						e: /\)/,
						k: EEL_KEYWORDS,
						i: /["']/
					}
				]
			},
			{
				cN: "function",
				bK: "procedure",
				e: /\s*\{/,
				eE: true,

				c:
				[
					e.TM,
					{
						cN: "params",
						b: /\(/,
						e: /\)/,
						k: EEL_KEYWORDS,
						i: /["']/
					}
				]
			}
		]
	}
});

hljs.initHighlightingOnLoad();