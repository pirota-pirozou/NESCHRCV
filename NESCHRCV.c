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

static int * pPal_order = NULL;

static char infilename[256];
static char outfilename[256];

static int main_result = 0;
static int spr_num;

static int pal_cou;

static int *pal_buf = NULL;
static u_char *outbuf = NULL;

PDIB dibbuf = NULL;

static int opt_b = 0;													/* ベタ出力オプション */
static int opt_d = 0;													/* デバッグオプション */
static int opt_p = 0;													/* パレット正規化オプション */
static int opt_n = 0;													/* パレット正規化オプション２ */

static int getfilesize(char *);
static int readjob(void);
static int cvjob(void);

// 使用法の表示
static void usage(void)
{
    printf("usage: NESCHRCV infile[" INFILE_EXT "] OutFile\n"\
		   "\t-b[1-256]\tパレット最適化なしで変換（省略時には256）\n" \
		   "\t-n\t色をパレット０(0-31)に正規化\n"
		   "\t-p\t64以降の色を32台に正規化（せさみ用）\n"
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

    if (argc <= 1)
        usage();

	for (i=1; i<argc; i++)
	{
		ch = argv[i][0];
		if (ch == '-' || ch == '/')
		{
			// スイッチ
			switch (argv[i][1])
			{
			case 'b':
				opt_b = atoi(argv[i]+2);
				if (opt_b == 0)
				{
					opt_b = 256;
				}
//				printf("opt_b=%d\n",opt_b);
				break;
			case 'd':
				opt_d = 1;
//				printf("opt_d\n");
				break;
			case 'p':
				opt_p = 1;
//				printf("opt_p\n");
				break;
			case 'n':
				opt_n = 1;
//				printf("opt_n\n");
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

	// 出力バッファの確保
	outbuf = (u_char *) malloc(64 * 256);
	if (outbuf == NULL)
	{
		printf("出力バッファは確保できません\n");
		goto cvEnd;
	}

	// パレット置換バッファの確保
	pPal_order = (int *) malloc(sizeof(int) * 256);
	if (pPal_order == NULL)
	{
		printf("パレット置換バッファは確保できません\n");
		goto cvEnd;
	}
	
	// パレットバッファの確保
	pal_buf = (int *) malloc(sizeof(int)*256);
	if (pal_buf == NULL)
	{
		printf("パレットバッファが確保できません\n");
		return -1;
	}
	memset(pal_buf, -1, sizeof(int)*256);

    // 変換処理
	if (cvjob() < 0)
	{
		goto cvEnd;
	}
    
cvEnd:
    // 後始末

	// パレットバッファ開放
	if (pal_buf != NULL)
	{
		free(pal_buf);
		pal_buf = NULL;
	}
	
	// パレット置換出力開放
	if (pPal_order != NULL)
	{
		free(pPal_order);
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

    // 色コード正規化
    if (opt_p) {
        for (yl=0; yl<bi->biHeight; yl++) {
            pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed) + (yl * bi->biWidth);
            for (xl=0; xl<bi->biWidth; xl++) {
                a = *pimg;
                // パレット１に強制（せさみ専用）
                if (a > 64) {
                    a = (a & 0x1f) | 0x20;
                    *pimg = a;
                }
                pimg++;
            }
        }

    } else // if (opt_p)
    if (opt_n) {
        //パレット正規化 0-31
        for (yl=0; yl<bi->biHeight; yl++) {
            pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed) + (yl * bi->biWidth);
            for (xl=0; xl<bi->biWidth; xl++) {
                a = *pimg;
                // パレット０に強制
                a &= 0x1f;
                *pimg = a;
                pimg++;
            }
        }

    } // if (opt_n)

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

///////////////////////////////
// スプライトデータに変換処理
///////////////////////////////
static int cvjob(void)
{
	BITMAPINFOHEADER *bi;
	int i,j;
	int xl,yl;
	int a;
	u_char *pimg;
	u_char *outptr;
	RGBQUAD *dibpal, *paltmp;
	FILE *fp;

	bi = (BITMAPINFOHEADER *)dibbuf;
	spr_num = (bi->biWidth >> 3) * (bi->biHeight >> 3);					// スプライト個数/

	// 透明色の設定
	if (!opt_b)
	{
		// パレット最適化
		pal_buf[0]=0;
		pPal_order[0]=0;
		pal_cou=1;
	}
	else
	{
		// パレット最適化無し
		for (i=0; i<256; i++)
		{
			pal_buf[i]=i;
			pPal_order[i]=i;
		}
		pal_cou=256;
	}

	// 変換処理
	outptr = outbuf;													// スプライト出力バッファの初期化
	for (yl=0; yl<bi->biHeight; yl+=8)
	{
		for (xl=0; xl<bi->biWidth; xl+=8)
		{
			pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed);
			pimg += ((yl * bi->biWidth) + xl);
			for (i=0; i<8; i++)
			{
				for (j=0; j<8; j++)
				{
					a = *(pimg++);
					if (pal_buf[a]>=0)
					{
						// 既に使用済みの色
						*(outptr++) = (u_char) pal_buf[a];
					}
					else
					{
						// 新規に見つけた色
						pal_buf[a] = pal_cou;							// 利用インデックス
						pPal_order[pal_cou] = a;						// 逆引き
						*(outptr++) = (u_char) pal_buf[a];
						pal_cou++;
					}
					
				}
				pimg += (bi->biWidth - 8);
			}
		}

	}

	// ファイル出力
	fp = fopen(outfilename,"wb");
	if (fp == NULL)
	{
		printf("Can't write '%s'.\n", outfilename);
		return -1;
	}

    if (opt_b > 0) {
        // ベタ出力オプション時
        if (pal_cou < opt_b) {
//            printf("warning:-b指定の数値(%d)が使用色数より多くなっています。\n",opt_b);
        } else {
            printf("パレット出力数を%dに設定します。\n",opt_b);
            pal_cou = opt_b;
        }
    }

    
	// パレット出力
	dibpal = (RGBQUAD *)((u_char *) dibbuf + sizeof(BITMAPINFOHEADER));
	fputc(pal_cou-1, fp);												// パレット個数
	for (i=0; i<pal_cou; i++)
	{
		paltmp = dibpal + pPal_order[i];
		fputc(paltmp->rgbRed, fp);										// パレット赤
		fputc(paltmp->rgbGreen, fp);									// パレット緑
		fputc(paltmp->rgbBlue, fp);										// パレット青
	}

	// パターン出力
	fputc(spr_num-1, fp);												// パターン個数
	a = fwrite(outbuf, 1, 64*spr_num, fp);
	if (a != (64*spr_num))
	{
		printf("'%s' ファイルが正しく書き込めませんでした！\n", outfilename);
	}

	fclose(fp);

	// 結果出力
	printf("パレット数:%d\n",pal_cou);
	printf("パターン数:%d\n",spr_num);
	printf("スプライトデータ '%s'を作成しました。\n", outfilename);

	return 0;
}
