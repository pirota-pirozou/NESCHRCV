/////////////////////////////////////////////////////////////////////////////
// NESCHRCV.c
// programmed by pirota
/////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "png.h"
#include "pngctrl.h"

#define INFILE_EXT  ".png"
#define OUTFILE_EXT ".chr"

#define PAL_LINES	64		// 最大パレット本数

#define swap_byte(a, b)	do { u_char tmp = *a; *a = *b; *b = tmp; } while(0)

typedef struct __pal_collection
{
	int		used;		// 使用された数
	u_char	pal[4];		// NESパレットインデックス

} TPAL_COLLECTIONS, *pTPAL_COLLCECTION;

static TPAL_COLLECTIONS nespal[PAL_LINES];

static u_char *outbuf = NULL;
static u_char *pat_ptr = NULL;

static char infilename[256];
static char outfilename[256];

static int main_result = 0;
static int spr_num;

static int pal_cou;

static int width;
static int height;

PDIB dibbuf = NULL;

static int opt_d = 0;													// デバッグオプション

static int now_line = 0;



static int getfilesize(char *);
static int readjob(void);
static int cvjob(void);

// 使用法の表示
static void usage(void)
{
    printf("usage: NESCHRCV infile[" INFILE_EXT "] OutFile\n"\
		   "\t-d\tDIBファイルも出力（デバッグ用）\n"
		   );

    exit(0);
}

// EDGEパレットインデックスからNESパレットインデックスに変換
static int edge2nes(int idx)
{
	int y = (idx >> 4);
	int x = (idx & 3);

	return (x << 4) + y;
}

// バブルソート
static void bubblesort(u_char *a)
{
	BOOL isEnd = FALSE;
	int finAdjust = 1;
	int i;
	while (isEnd == FALSE)
	{
		BOOL wasSwap = FALSE;
		for (i = 0; i < 4 - finAdjust; i++)
		{
			if (a[i+1] < a[i])
			{
				swap_byte(&(a[i+1]), &(a[i]));
				wasSwap = TRUE;
			}
		}
		if (!wasSwap)
		{
			isEnd = TRUE;
		}
		finAdjust++;
	}
}

/////////////////////////////////////
// main
/////////////////////////////////////
int main(int argc, char *argv[])
{
	int i;
	int ch;
	
    char drive[ _MAX_DRIVE ];
    char dir[ _MAX_DIR ];
    char fname[ _MAX_FNAME ];
    char ext[ _MAX_EXT ];

	// コマンドライン解析
	memset(infilename, 0, sizeof(infilename) );
	memset(outfilename, 0, sizeof(outfilename) );
	
    printf("PNG to NESCHR Converter Ver0.00 " __DATE__ "," __TIME__ " Programmed by pirota\n");

	//----------- bubble sort test --------------
//	static u_char test[] = { 0, 3, 4, 1, 0 };
//	bubblesort(test);

    if (argc <= 1)
	{
		usage();
	}
        

	for (i=1; i<argc; i++)
	{
		ch = argv[i][0];
		if (ch == '-' || ch == '/')
		{
			// スイッチ
			switch (argv[i][1])
			{
			case 'd':
				opt_d = 1;
//				printf("opt_d\n");
				break;
			default:
				printf("-%c オプションが違います。\n",argv[i][1]);
				break;
			}

			continue;
		}
		// ファイル名入力
		if (!infilename[0])
		{
			strcpy(infilename, argv[i]);
			_splitpath(infilename , drive, dir, fname, ext );
			if (ext[0]==0)
				strcat(infilename, INFILE_EXT);							// 拡張子補完

			continue;
		}
		
		// 出力ファイル名の作成
		if (!outfilename[0])
		{
			// 出力ファイルネーム
			strcpy(outfilename, argv[i]);
		}
		
	}
	// 出力ファイル名が省略されてたら
	if (!outfilename[0])
	{
		// 出力ファイルネーム
		sprintf(outfilename, "nes.chr");
	}
	
    // ファイル読み込み処理
    if (readjob()<0)
		goto cvEnd;


	// 各種バッファの確保

	// NESパレットのワーク
	memset(nespal, 0, sizeof(nespal));


	// 出力バッファの確保
	outbuf = (u_char *) malloc((width/8) * (height*2));
	if (outbuf == NULL)
	{
		printf("出力バッファは確保できません\n");
		goto cvEnd;
	}

    // 変換処理
	if (cvjob() < 0)
	{
		goto cvEnd;
	}
    
cvEnd:
	// 後始末
	// NESパターン出力バッファ開放
	if (pat_ptr != NULL)
	{
		free(pat_ptr);
	}

	// スプライト出力バッファ開放
	if (outbuf != NULL)
	{
		free(outbuf);
	}

	// パックバッファ開放
	if (dibbuf != NULL)
	{
		free(dibbuf);
	}

	return main_result;
}

//----------------------
// ファイルサイズを取得
//----------------------
static int getfilesize(char *fname)
{
    int result = -1;
    FILE * fp;

    fp = fopen(fname, "rb");
    if (fp == NULL)
        return result;

    fseek(fp, 0, SEEK_END);
    result = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fclose(fp);

    return result;
}

//----------------------------
// ＰＮＧ読み込みＤＩＢに変換
//----------------------------
// Out: 0=ＯＫ
//      -1 = エラー
static int readjob(void)
{
	FILE *fp;
	BITMAPFILEHEADER bf;
	BITMAPINFOHEADER *bi;
	u_char *pimg;
	int bytes;
	int xl, yl;
    u_char a;
    
	dibbuf = PngOpenFile(infilename);
	if (dibbuf == NULL)
	{
		printf("Can't open '%s'.\n", infilename);
		return -1;
	}

	bi = (BITMAPINFOHEADER *)dibbuf;
	// テスト
	memset(&bf, 0, sizeof(bf));
	bf.bfType = 'MB';
	bf.bfSize = sizeof(bf);
	bf.bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed;

	width = bi->biWidth;
	height = bi->biHeight;

	if (width != 128 || height != 128)
	{
		printf("Error:画像サイズは128x128px しか受け付けません。\n");
		return -1;
	}

	if (bi->biBitCount != 8)
	{
		printf("Error:画像ファイルは インデックスカラー しか受け付けません。\n");
		return -1;
	}

	// EDGE→NES パレットへ変換
	for (yl=0; yl<height; yl++)
	{
		pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed) + (yl * width);
		for (xl=0; xl<width; xl++)
		{
			a = *pimg;
			a = edge2nes(a);
			*pimg = a;
			pimg++;
		}
	}

	pat_ptr = (u_char *)malloc(width*height);
	memset(pat_ptr, 0, width*height);

	if (opt_d)
	{
		// デバッグオプションがonならDIBファイル出力
		fp = fopen("test.bmp" , "wb");
		fwrite(&bf, 1, sizeof(bf), fp);
		
		bytes = bi->biSizeImage + (bi->biClrUsed * sizeof(RGBQUAD)) + sizeof(BITMAPINFOHEADER);
		
		fwrite(dibbuf, 1, bytes, fp);
		
		fclose(fp);
		printf("debug: 'test.bmp' wrote.\n");
	}

	return 0;
}

//
static void search_reg_palet(u_char *ptr, int stride)
{
	int i, j, k;
	int found;
	u_char * pimg = ptr;
	u_char a;

	TPAL_COLLECTIONS paltmp;
	memset(&paltmp, 0, sizeof(paltmp));

	// 16x16で使っている色を調べる
	// 16x16 px 1block
	for (i = 0; i < 16; i++)
	{
		for (j = 0; j < 16; j++, pimg++)
		{
			a = *(pimg);
			if (a == 0)
			{
				continue;
			}
			found = 0;
			// 現在のパレットラインの検索
			for (k = 1; k < 4; k++)
			{
				if (a == paltmp.pal[k])
				{
					// 使用済みの色
					found = k;
					break;
				}
			}
			// 色が見つからなかった場合、新規登録
			if (found == 0)
			{
				k = (++paltmp.used);
				if (k >= 4)
				{
					// パレットオーバーフロー
					printf("warning: パレットの使用数が3色を越えています。\n");
					k =	(--paltmp.used);
				}
				// 新規色の登録
				paltmp.pal[k] = a;
			}

		} // j
		pimg += (stride - 16);

	}  // i
	// パレットのハッシュ作成
	bubblesort(paltmp.pal);

	// ハッシュに合わせてインデックス化する
	pimg = ptr;
	for (i = 0; i < 16; i++)
	{
		for (j = 0; j < 16; j++, pimg++)
		{
			a = *(pimg);
			if (a == 0)
			{
				continue;
			}
			found = 0;
			// 現在のパレットラインの検索
			for (k = 1; k < 4; k++)
			{
				if (a == paltmp.pal[k])
				{
					// 使用済みの色
					found = k;
					break;
				}
			}

			// パレットをインデックスに置換
			*pimg = (u_char)found;

		} // j
		pimg += (stride - 16);
	} // i

	// 新規ハッシュの時のみパレット登録
	DWORD hash1 = *((DWORD *)&paltmp.pal);
	DWORD hash2 = *((DWORD *)&nespal[now_line].pal);
	if (hash1 != hash2)
	{
		if (now_line >= PAL_LINES)
		{
			printf("Error: パレット行数が %d を越えました。", PAL_LINES);
		}
		else
		{
			nespal[now_line++] = paltmp;
			printf("\t.byte\t");
			for (i = 0; i < 4; i++)
			{
				printf("%2d", nespal[now_line - 1].pal[i]);
				if (i < 3)
				{
					putchar(',');
				}
				else
				{
					puts("");
				}
			}

		}
	}


}

///////////////////////////////
// スプライトデータに変換処理
///////////////////////////////
static int cvjob(void)
{
	BITMAPINFOHEADER *bi;
	int xl,yl;
	u_char *pimg;
	u_char *outptr;
	FILE *fp;

	bi = (BITMAPINFOHEADER *)dibbuf;
	spr_num = (width >> 3) * (height >> 3);					// スプライト個数

	// 変換処理
	outptr = outbuf;										// スプライト出力バッファの初期化

	pimg = NULL;
	for (yl=0; yl<height; yl+=16)
	{
		for (xl=0; xl<width; xl+=16)
		{
			pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed);
			pimg += ((yl * bi->biWidth) + xl);
			search_reg_palet(pimg, width);
		} // xl

		pimg += (width - 16);

	} // yl

	printf("パレット本数；%d\n", now_line);


	// ファイル出力
	fp = fopen(outfilename,"wb");
	if (fp == NULL)
	{
		printf("Can't write '%s'.\n", outfilename);
		return -1;
	}
    
	// パレット出力
#if 0
	dibpal = (RGBQUAD *)((u_char *) dibbuf + sizeof(BITMAPINFOHEADER));
	fputc(pal_cou-1, fp);												// パレット個数
	for (i=0; i<pal_cou; i++)
	{
		paltmp = dibpal + pPal_order[i];
		fputc(paltmp->rgbRed, fp);										// パレット赤
		fputc(paltmp->rgbGreen, fp);									// パレット緑
		fputc(paltmp->rgbBlue, fp);										// パレット青
	}
#endif

	// パターン出力
#if 0
	fputc(spr_num-1, fp);												// パターン個数
	a = fwrite(outbuf, 1, 64*spr_num, fp);
	if (a != (64*spr_num))
	{
		printf("'%s' ファイルが正しく書き込めませんでした！\n", outfilename);
	}
#endif
	fclose(fp);

	// 結果出力
	printf("パレット数:%d\n",pal_cou);
	printf("パターン数:%d\n",spr_num);
	printf("スプライトデータ '%s'を作成しました。\n", outfilename);

	return 0;
}
