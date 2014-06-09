# Praise SeanWu m(_ _)m
## [Code] MPEG-1
差不多看完spec了，附帶一些超廢的筆記
總之 part1 system 那份應該用不到，因為助教給的是 .m1v
雖然不太清楚不過應該是直接 video sequence，沒有 system layer

// 檔案開頭就 000001B3 了，這是 sequence_header_code (video那份2.4.2)
// 如果是 system 開始應該是 pack_start_code=000001BA

所以 part2 video 那份文件就夠了
重點大約是前 50 頁左右 (中間附帶名詞解釋+目錄+表格之類的，沒那麼多 XD)
從 2.4.2 檔案結構開始看，需要對照 2.4.3，符號和變數看不懂去查 2.1~2.2

到這邊應該需要的 data (frame,color,motion vector... etc) 都可以解得出來
然後這些東西要怎麼組合回影片主要在 2.4.4 這樣 (有些散在前面)

之後的內容 A-1 說 Inverse-DCT 你應該去看哪邊 blabla
B-XX 是一堆表格，decode要用的
更後面主要跟 encode 有關，基本上應該不用管
p.D-7 有個 decoder 的 block diagram 不過應該沒什麼屁用

2-D.4 MODEL DECODER 那邊不知道在幹嘛，跟前面 2.3~2.4 超像
或許有更多一些細節不過我覺得 redundancy 異常多，連表格都重覆了
到時候寫一寫卡關了再來看看好了
--
※ 發信站: 批踢踢兔(ptt2.cc)
◆ From: 140.112.250.122
→ LPH66:要注意解出來的影像顏色對不對  我當年demo時在這裡炸掉.. 推 06/09 00:45
→ LPH66:聽助教講好像是 Cr 跟 Cb 搞錯了的樣子                   推 06/09 00:45


## Re: [Code] MPEG-1
注意 dct_coeff_first 和 dct_coeff_next
用的 Huffman table 是不一樣的!
見 p.B-6 NOTE2, NOTE3，目地是為了迴避 end_of_block '10' (p.34)
總之那個 table 小細節有點多，要仔細看

## Re: [Code] MPEG-1
pattern_code[0~5] 的取法，在 p.41
不過雖然它被放在 coded_block_pattern 這一小節下
沒有 coded_block_pattern 時它的值有可能非 0 (因macroblock_intra!=0)

要記得補，不然就是改一下 block() 區塊的結構，
macroblock_intra!=0 時分開做之類的

--
※ 發信站: 批踢踢兔(ptt2.cc)
◆ From: 140.112.4.194
→ peer4321:先設cbp=0再檢查coded_block_pattern，                推 06/19 08:54
→ peer4321:if完for之前更新pattern_code[i]？

## Re: [Code] MPEG-1
非 intra macroblock:

motion vector 是以 macroblock 為單位，
每個 macroblock 有一個 motion vector ( 沒給的話就是 (0,0) )
但每個小 block 會有自己的 dct 系數 (dct_recon[][])

intra macroblock: 直接 call I-frame 的 macroblock decoder 來解

1. 那些沒有出現在 pattern_code[] 裡的 block，記得 dct_zz 要歸 0

2. past_intra_address != macroblock_address - increase
   所以每次 address_increase>1 時重設 dct_dc_xx_past 是錯的!
   請照 2.4.4.1 的方法好好的實作，記得進 slice() 時設成 -2

3. 沒給 motion vector 時，motion vector 要歸 0
--
※ 發信站: 批踢踢兔(ptt2.cc)
◆ From: 140.112.30.81
→ seanwu:還沒處理 skipped macroblock:                          推 06/13 21:46
→ seanwu:http://w.csie.org/~b98902113/IP_1.mp4                 推 06/13 21:46
→ seanwu:對了，上一個IPB_ALL.mp4是有bug的，就是因為            推 06/13 21:48
→ seanwu:past_intra_address 沒有處理好，那些一大片紅色藍色的   推 06/13 21:49
→ seanwu:我猜原本應該會是正確的影像，因為是 intra_macroblock   推 06/13 21:49

## Re
P-frame 裡 skipped macroblock 的處理方法是這樣

    //....
    for( int i=0; i<macroblock_address_increment-1; i++ ){
        macroblock_address++;
        mb_row = macroblock_address / mb_width;
        mb_column = macroblock_address % mb_width;
        if(picture_coding_type==PICTURE_TYPE_P){
            macroblock_skipped_P();
            //這邊很簡單，就是直接把上一張圖的同一個macroblock copy過來
        }
        recon_down_for_prev = recon_right_for_prev = 0;
        //我不確定B-frame裡是不是要這樣處理，或許該放到上面的if裡
    }
    macroblock_address++;
    mb_row = macroblock_address / mb_width;
    mb_column = macroblock_address % mb_width;
    //接這個macroblock() 原本要做的內容: ...
--
※ 發信站: 批踢踢兔(ptt2.cc)
◆ From: 140.112.30.81
→ seanwu:對了，P-frame做完後基本上可以直接播了，不用buffer     推 06/13 21:59
→ seanwu:recon_down/right_for_prev 確實該放進去                推 06/16 14:12
→ seanwu:B-frame的skip macroblock是要吃之前的motion vecter     推 06/16 14:13

## Re
到P-frame為止，macroblock() 裡應該會有:

    if(picture_coding_type==PICTURE_TYPE_P){
        calc_motion_vector_P();
    }
    for( int i=0; i<6; i++ ){
        block(i);
        if(picture_coding_type==PICTURE_TYPE_I){
            intra_coded_block_decode(i);
        }else if(picture_coding_type==PICTURE_TYPE_P){
            if(macroblock_intra){
                intra_coded_block_decode(i);
            }else{
                predictive_block_P_decode(i);
            }
        }
    }

## Re
解是解完了啦... 圖跟我用其它軟體輸出的連續圖一樣
只是... 好像亮了一點 = =   不知道為啥
--
※ 發信站: 批踢踢兔(ptt2.cc)
◆ From: 140.112.250.122
→ seanwu:噢搞定了，我用的 YCbCr -> RGB 是錯的，跟jpeg不一樣    推 06/17 02:02
→ seanwu:http://en.wikipedia.org/wiki/YCbCr                    推 06/17 02:02
→ seanwu:要用 ITU-R BT.601 conversion                          推 06/17 02:02
