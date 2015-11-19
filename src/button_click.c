
#include "pebble.h"

#define PERSIST_KEY_SCORE 772

#define ENGINE_TILE_SIZE 18
#define ENGINE_TILE_NOX 8
#define ENGINE_TILE_NOXH 4
#define ENGINE_HIGHSCORE_OFFS 64
#define ENGINE_TILEMAPWIDTH 24
#define ENGINE_PLAYER_STARTX 12
#define ENGINE_PLAYER_STARTY 12
#define ENGINE_PLAYER_STARTDX 0
#define ENGINE_PLAYER_STARTDY 1
#define ENGINE_FONT_HEIGHT 18
#define ENGINE_FONT_SPACING 2

#define GColorBlackARGB8 ((uint8_t)0b11000000)

static Window *s_main_window;
static Layer *s_image_layer;
static GBitmap *s_image;
static GBitmap *t_image[84];
static GBitmap *p_sel;
static GBitmap *p_walk;
static unsigned char tilemap[ENGINE_TILEMAPWIDTH*ENGINE_TILEMAPWIDTH];
static unsigned char hiscore[25];
static unsigned char score[5];

static int StartTimer=25;
static int ClockSpeed=1;
static int ClockCounter=0;
static int StartX=0;
static int StartY=0;
static int GoalX=0;
static int GoalY=0;
static int Obstructions=0;

static unsigned char rlist[4]={1,2,3,4} ;

// Player coordinate and player direction
static int px;
static int py;
static int pdx;
static int pdy;
static int state=2;
int level=0;

// Current processing Coordinate
static int cpx=10;
static int cpy=10;

// Bitmap garbage collection
int bitmapcnt=0;

// Animation Timer
AppTimer *timer;
const int delta = 500;

#define starttile 9

// Tile redirection array (do not morph 0, normal tiles start at 1)
static int redirect_tile[84] = { 0, 
															  1,  9, 17, 25,  1, 33, 41, 25,			// Vertical (1-4) (5-8)
															  2, 10, 18, 26,  2, 34, 42, 26,			// Horizontal (9-12) (13-16)
																3, 11, 19, 27,  3, 35, 43, 27,      // Bottom Right (17-20) (21-24)
																4, 12, 20, 28,  4, 36, 44, 28,      // Bottom Left (25-28) (29-32)
																5, 13, 21, 29,  5, 37, 45, 29,      // Top Right (33-36) (37-40)
																6, 14, 22, 30,  6, 38, 46, 30,      // Top Left (41-44) (45-48)
																7, 15, 23, 31,  7, 39, 47, 31,      // Crossing Left To Right Empty Vert (49-52) (53-56)
															 54, 51, 59, 61, 54, 52, 60, 61,      // Crossing Left to Right Full Vert (57-60) (61-64)
															  7, 63, 63, 54,  7, 55, 55, 54,      // Crossing Top to Bottom Empty Horizontal (65-68) (69-72)
															 31, 53, 53, 61, 31, 62, 62, 61,      // Crossing Top to Bottom Full Horizontal (73-76) (77-80)
															 8, 16, 24                            // Wall Tile, Start Tile and End Tile
															 };

// Font character redirection
static int font_characters[26]={
		50,78,74,0,56,0,77,0,80,0,
		0,57,79,58,48,76,81,75,40,0,
	  49,82,0,0,0,0
};

// Per character font widths A-J K-T U-Z 
static int font_width[26]={
		9,9,9,9,9,9,9,9,4,9,
		9,9,15,9,9,9,5,9,9,9,
	  9,9,9,9,9,9
};

//---------------------------------------------------------------------------------//
// writestring
//---------------------------------------------------------------------------------//
// Proportional Width Text Writer
//---------------------------------------------------------------------------------//

void writestring(GContext* ctx,char *str,int sx,int sy)
{	
		int len=strlen(str);
		int xk=sx;
		for(int i=0;i<len;i++){
				int chr=str[i]-65;
				if(font_characters[chr]!=0){
						graphics_draw_bitmap_in_rect(ctx, t_image[font_characters[chr]], GRect(xk,sy, font_width[chr], ENGINE_FONT_HEIGHT));						
				}
				xk+=font_width[chr]+ENGINE_FONT_SPACING;	
		}
}

// Garbage collected sprite creation (reference counter only)
static GBitmap* makeSprite(const GBitmap * base_bitmap, GRect sub_rect)
{
	bitmapcnt++;
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "Loop index now %d", bitmapcnt);
	return gbitmap_create_as_sub_bitmap(base_bitmap,sub_rect);
}

//---------------------------------------------------------------------------------//
// comparescore
//---------------------------------------------------------------------------------//
// Returns true if current score is higher than high score
//---------------------------------------------------------------------------------//

static int comparescore()
{
		int scored=(score[0])+(10*score[1])+(100*score[2])+(1000*score[3])+(1000*score[4]);
		int scorel=(hiscore[(level*5)+0])+(10*hiscore[(level*5)+1])+(100*hiscore[(level*5)+2])+(1000*hiscore[(level*5)+3])+(1000*hiscore[(level*5)+4]);	
		
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Loop index now %d %d", scored, scorel);
	
		return (scored>scorel);
}

//---------------------------------------------------------------------------------//
// timer_callback
//---------------------------------------------------------------------------------//
// Called when the graphics need to be updated
//---------------------------------------------------------------------------------//

void timer_callback(void *data) {
		if(state==1){
			// Main Game State
				// Update Timer	
				ClockCounter++;
				if(((ClockCounter%ClockSpeed)==0)&&StartTimer>0) StartTimer--;	

				// Simple float code
				unsigned char ctile=tilemap[(cpy*ENGINE_TILEMAPWIDTH)+cpx];

				// Tile delta
				int cpxd=0;
				int cpyd=0;
				unsigned char ntile=0;

				if(StartTimer==0){
						// This complicated tangle of if-statements handles the pipe updating operations.
						// One statement for each end tile (if it it not an and tile then advance tile)
						// A is Below, B is Left, C is Right, and D is Above
						if(ctile==4||ctile==20||ctile==28||ctile==68||ctile==76){					//----===## A ##===----
								cpyd=1;
								ntile=tilemap[((cpy+cpyd)*ENGINE_TILEMAPWIDTH)+(cpx+cpxd)];
								if(ntile==1||ntile==33||ntile==41||ntile==65||ntile==73){     // Normal tile - do nothing - let engine continue filling pipes
								}else if(ntile==49){ 		                                      // Tile that needs to flip direction
										ntile=65;
								}else{
									// Game over!
									// APP_LOG(APP_LOG_LEVEL_DEBUG, "GAME OVER A %d",ntile);
								}
								tilemap[((cpy+cpyd)*ENGINE_TILEMAPWIDTH)+(cpx+cpxd)]=ntile;
						}else if(ctile==16||ctile==32||ctile==44||ctile==56||ctile==64){	//----===## B ##===----
								cpxd=-1;
								ntile=tilemap[((cpy+cpyd)*ENGINE_TILEMAPWIDTH)+(cpx+cpxd)];
								if(ntile==13||ntile==17||ntile==37||ntile==53||ntile==61){    // Normal tile - do nothing - let engine continue filling pipes
								}else if(ntile==9||ntile==33||ntile==49){                     // Tile that needs to flip direction
										ntile+=4;
								}else if(ntile==68||ntile==72){                               // Vertical part 
										ntile=61;
								}else{
									// Game over!
									// APP_LOG(APP_LOG_LEVEL_DEBUG, "GAME OVER B %d",ntile);
								}
								tilemap[((cpy+cpyd)*ENGINE_TILEMAPWIDTH)+(cpx+cpxd)]=ntile;
						}else if(ctile==12||ctile==24||ctile==36||ctile==52||ctile==60){	//----===## C ##===----
								cpxd=1;
								ntile=tilemap[((cpy+cpyd)*ENGINE_TILEMAPWIDTH)+(cpx+cpxd)];
								if(ntile==9||ntile==21||ntile==33||ntile==49||ntile==57||ntile==25||ntile==49){  // Normal tile - do nothing - let engine continue filling pipes
								}else if(ntile==41){ 		                // Tile that needs to flip direction
										ntile=45;
								}else if(ntile==68||ntile==72){          										// Horizontal part has been filled
										ntile=57;
								}else{
									// Game over!
									// APP_LOG(APP_LOG_LEVEL_DEBUG, "GAME OVER C %d",ntile);
								}
								tilemap[((cpy+cpyd)*ENGINE_TILEMAPWIDTH)+(cpx+cpxd)]=ntile;
						}else if(ctile==8||ctile==40||ctile==48||ctile==72||ctile==80){		//----===## D ##===----
								cpyd=-1;
								ntile=tilemap[((cpy+cpyd)*ENGINE_TILEMAPWIDTH)+(cpx+cpxd)];
								if(ntile==5||ntile==37||ntile==45||ntile==69||ntile==77){    // Normal tile - do nothing - let engine continue filling pipes
								}else if(ntile==1||ntile==17||ntile==25){ 		               // Tile that needs to flip direction
										ntile+=4;
								}else if(ntile==49){ 		                 										// Tile that needs to flip direction
										ntile=69;
								}else if(ntile==52||ntile==56){          										// Horizontal part has been filled
										ntile=77;
								}else{
									// Game over!
									// APP_LOG(APP_LOG_LEVEL_DEBUG, "GAME OVER D %d",ntile);
								}
								tilemap[((cpy+cpyd)*ENGINE_TILEMAPWIDTH)+(cpx+cpxd)]=ntile;
						}else{
								ctile ++;			
						}

						tilemap[(cpy*ENGINE_TILEMAPWIDTH)+cpx]=ctile;

						// if we changed the tile
						if(ntile>0){
								cpx+=cpxd;
								cpy+=cpyd;
						}
				}

				// Update Graphics
				layer_mark_dirty(s_image_layer);
		}

    //Register next execution
    timer = app_timer_register(delta, (AppTimerCallback) timer_callback, NULL);
}

//---------------------------------------------------------------------------------//
// initGameBoard
//---------------------------------------------------------------------------------//
// Clear playfield and init scoreboard
//---------------------------------------------------------------------------------//

static void initGameBoard()
{
	// Read High Score.... from persistent storage (or clear it)
	if (persist_exists(PERSIST_KEY_SCORE)) {
				persist_read_data(PERSIST_KEY_SCORE, hiscore, 25);
	}else{
			for(int i=0;i<25;i++){
					hiscore[i]=0;
			}			
	}

	// Clear Score
	for(int i=0;i<5;i++){
			score[i]=0;
	}	
	
	// Assign some data to tile map
	for(int i=0;i<(ENGINE_TILEMAPWIDTH*ENGINE_TILEMAPWIDTH);i++){
			tilemap[i]=0;		
	}

	// Clear all edges of tilemap
	for(int i=0;i<ENGINE_TILEMAPWIDTH;i++){
			tilemap[i]=81;
			tilemap[i*ENGINE_TILEMAPWIDTH]=81;
			tilemap[((ENGINE_TILEMAPWIDTH-1)*ENGINE_TILEMAPWIDTH)+i]=81;
			tilemap[(i*ENGINE_TILEMAPWIDTH)+ENGINE_TILEMAPWIDTH-1]=81;
	}
	
	// Move player to center of tile map
	px=ENGINE_PLAYER_STARTX;
	py=ENGINE_PLAYER_STARTY;
	pdx=ENGINE_PLAYER_STARTDX;
	pdy=ENGINE_PLAYER_STARTDY;
	
	// Randomize Future Tiles
	for(int i=0;i<4;i++){
			rlist[i]=1+(rand()%7);
	}

	// Make the level parameters
	if(level==0){
			StartTimer=99;
			ClockSpeed=6;
			StartX=8+(rand()%8);
			StartY=8+(rand()%8);
			GoalX=12-StartX+12;
			GoalY=12-StartY+12;
			Obstructions=0;
	}else if(level==1){
			StartTimer=99;
			ClockSpeed=4;
			StartX=6+(rand()%12);
			StartY=6+(rand()%12);
			GoalX=12-StartX+12;
			GoalY=12-StartY+12;
			Obstructions=10;
	}else if(level==2){
			StartTimer=99;
			ClockSpeed=2;
			StartX=4+(rand()%16);
			StartY=4+(rand()%16);
			GoalX=12-StartX+12;
			GoalY=12-StartY+12;
			Obstructions=30;
	}else if(level==3){
			StartTimer=50;
			ClockSpeed=2;
			StartX=4+(rand()%16);
			StartY=4+(rand()%16);
			GoalX=12-StartX+12;
			GoalY=12-StartY+12;
			Obstructions=60;	
	}else if(level==4){
			StartTimer=50;
			ClockSpeed=1;
			StartX=4+(rand()%16);
			StartY=4+(rand()%16);
			GoalX=12-StartX+12;
			GoalY=12-StartY+12;
			Obstructions=120;
	}

	// Place Start and Goal Tiles in tilemap and move start of computation to right place
	tilemap[(ENGINE_TILEMAPWIDTH*StartY)+StartX]=82;
	tilemap[(ENGINE_TILEMAPWIDTH*GoalY)+GoalX]=83;	
	cpx=StartX-1;
	cpy=StartY;
	
	// Randomize un-navigable obstructions.
	int tx,ty;
	for(int i=0;i<Obstructions;i++){
			tx=2+(rand()%20);
			ty=2+(rand()%20);
			tilemap[(tx*24)+ty]=81;
	}
	
}

//---------------------------------------------------------------------------------//
// up_click_handler
//---------------------------------------------------------------------------------//
// Long Select Handler
//---------------------------------------------------------------------------------//

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
		// Only in game mode 0.5s click time or more	
		if(state==1){
				// Lay pipe and advance
				tilemap[(py*ENGINE_TILEMAPWIDTH)+px-1]=((rlist[0]-1)*8)+1;
				for(int i=1;i<4;i++){
					rlist[i-1]=rlist[i];							
				}
				rlist[3]=1+(rand()%7);
		
				// Place new tile and move in direction of that tile
			 
		}
}

//---------------------------------------------------------------------------------//
// up_click_handler
//---------------------------------------------------------------------------------//
// Called when select is clicked
//---------------------------------------------------------------------------------//

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
		
		int currtile;
		
		if(state==0){
				state=1;
				initGameBoard();
		}	else if(state==1){
				// Only move if allowed
				if(((px+pdx)<(ENGINE_TILEMAPWIDTH-1))&&((px+pdx)>0)) px+=pdx;
				if(((py+pdy)<(ENGINE_TILEMAPWIDTH-1))&&((py+pdy)>0)) py+=pdy;
		}else if(state==2){
				state=0;
		}

		APP_LOG(APP_LOG_LEVEL_DEBUG, "px: %d py: %d pdx: %d pdy: %d", px,py,pdx,pdy);
	
		// Redraw Graphics
    layer_mark_dirty(s_image_layer);
}

//---------------------------------------------------------------------------------//
// down_click_handler
//---------------------------------------------------------------------------------//
// Called when down is clicked
//---------------------------------------------------------------------------------//

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	if(state==0){
			level++;
			if(level==5) level=0;
	}else	if(state==1){
				if(pdx==1){
						pdx=0;
						pdy=1;
				}else if(pdx==-1){
						pdx=0;
						pdy=-1;
				}else if(pdy==1){
						pdx=-1;
						pdy=0;
				}else if(pdy==-1){
						pdx=1;
						pdy=0;
				}
		}
    layer_mark_dirty(s_image_layer);
}

//---------------------------------------------------------------------------------//
// up_click_handler
//---------------------------------------------------------------------------------//
// Called when up is clicked
//---------------------------------------------------------------------------------//

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
	if(state==0){
			level--;
			if(level==-1) level=4;
	}else if(state==1){
				if(pdx==1){
						pdx=0;
						pdy=-1;
				}else if(pdx==-1){
						pdx=0;
						pdy=1;
				}else if(pdy==1){
						pdx=1;
						pdy=0;
				}else if(pdy==-1){
						pdx=-1;
						pdy=0;
				}
		}
    layer_mark_dirty(s_image_layer);
}

//---------------------------------------------------------------------------------//
// layer_update_callback
//---------------------------------------------------------------------------------//
// Called when the graphics need to be updated
//---------------------------------------------------------------------------------//

static void layer_update_callback(Layer *layer, GContext* ctx) {
	
	if(state==0){
			// Draw Logo and High Scores
			graphics_draw_bitmap_in_rect(ctx, t_image[37], GRect(0,0, 144, 42));			
			
			// Draw High Scores for all 5 levels
			for(int i=0;i<5;i++){

					graphics_draw_bitmap_in_rect(ctx, t_image[hiscore[i]+24], GRect(130-(i*14),42, 14, 24));
					graphics_draw_bitmap_in_rect(ctx, t_image[hiscore[i+5]+24], GRect(130-(i*14),66, 14, 24));
					graphics_draw_bitmap_in_rect(ctx, t_image[hiscore[i+10]+24], GRect(130-(i*14),90, 14, 24));
					graphics_draw_bitmap_in_rect(ctx, t_image[hiscore[i+15]+24], GRect(130-(i*14),114, 14, 24));
					graphics_draw_bitmap_in_rect(ctx, t_image[hiscore[i+20]+24], GRect(130-(i*14),138, 14, 24));
					
					graphics_draw_bitmap_in_rect(ctx, t_image[34], GRect(0,42+(i*24), 34, 24));
					graphics_draw_bitmap_in_rect(ctx, t_image[34], GRect(i*30,162, 30, 6));
 					graphics_draw_bitmap_in_rect(ctx, t_image[34], GRect(62,42+(i*24), 12, 24));
					graphics_draw_bitmap_in_rect(ctx, t_image[35], GRect(34,42+(i*24), 14, 24));
					graphics_draw_bitmap_in_rect(ctx, t_image[i+25], GRect(48,42+(i*24), 14, 24));
			}
			graphics_draw_bitmap_in_rect(ctx, t_image[38], GRect(0,42+(level*24), 34, 24));		
	}else if(state==1){
			// Update Tilemap
			int cx;
			int cy;
			int ccx;
			int tileno;
			int tx=0;
			int ty=0;

			// Current on-screen cursorcoordinate if outside box reset
			cx=px-4;
			cy=py-4;
			if(cx<0) cx=0;
			if(cx>(ENGINE_TILEMAPWIDTH-ENGINE_TILE_NOX)) cx=(ENGINE_TILEMAPWIDTH-ENGINE_TILE_NOX);
			if(cy<0) cy=0;
			if(cy>(ENGINE_TILEMAPWIDTH-ENGINE_TILE_NOX)) cy=(ENGINE_TILEMAPWIDTH-ENGINE_TILE_NOX);	

			// Y coordinate first, loop over x coordinate
			for(int j=0;j<ENGINE_TILE_NOX;j++){
				ccx=cx;
				for(int i=0;i<ENGINE_TILE_NOX;i++){
						tileno=tilemap[(cy*ENGINE_TILEMAPWIDTH)+ccx];
	
						// Morph tile number to graphical tile representation!
						tileno=redirect_tile[tileno];
						graphics_draw_bitmap_in_rect(ctx, t_image[tileno], GRect(i*ENGINE_TILE_SIZE,j*ENGINE_TILE_SIZE, ENGINE_TILE_SIZE, ENGINE_TILE_SIZE));
						ccx++;
						
						// Assign Coordinates
						if((ccx==px)&&(cy==py)){
								tx=i;
								ty=j;
						}
				}
				cy++;
			}

			// Draw Future Tiles
			for(int i=0;i<4;i++){
					graphics_draw_bitmap_in_rect(ctx, t_image[rlist[i]], GRect(50+(i*22),146, 18, 18));
			}		
			
			graphics_draw_bitmap_in_rect(ctx, t_image[64+(StartTimer/10)], GRect(16,144, 14, 24));
			graphics_draw_bitmap_in_rect(ctx, t_image[64+(StartTimer%10)], GRect(30,144, 14, 24));

//			graphics_draw_bitmap_in_rect(ctx, t_image[50], GRect(100,100, 18, 18));
//			graphics_draw_bitmap_in_rect(ctx, t_image[78], GRect(100,120, 18, 18));
//			graphics_draw_bitmap_in_rect(ctx, t_image[79], GRect(100,130, 18, 18));
		
			// Draw current position
			graphics_context_set_compositing_mode(ctx, GCompOpSet);
			graphics_draw_bitmap_in_rect(ctx,p_walk, GRect((tx+pdx)*ENGINE_TILE_SIZE,(ty+pdy)*ENGINE_TILE_SIZE,18,18));
			graphics_draw_bitmap_in_rect(ctx,p_sel, GRect(tx*ENGINE_TILE_SIZE,ty*ENGINE_TILE_SIZE,18,18));
			graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	}else if(state==2){
			// Clear Screen to Black Tile
			graphics_draw_bitmap_in_rect(ctx, t_image[32], GRect(0,0,144,168));

			// Bonus Counter
			writestring(ctx,"SCOREQ",40,100);

	}
}

//---------------------------------------------------------------------------------//
// click_config_provider
//---------------------------------------------------------------------------------//
// Creates all click event handlers
//---------------------------------------------------------------------------------//

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);

}

//---------------------------------------------------------------------------------//
// main_window_load
//---------------------------------------------------------------------------------//
// Called when window is defined (i.e. once per application start)
//---------------------------------------------------------------------------------//

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(s_main_window);
  GRect bounds = layer_get_frame(window_layer);

  s_image_layer = layer_create(bounds);
	
  layer_set_update_proc(s_image_layer, layer_update_callback);
  layer_add_child(window_layer, s_image_layer);
		
	// Define bitmap
  s_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NO_LITTER);

	// Define tiles (2 rows of 12 tiles at 12x12 px and 1 row of 14x23)
	// Define tiles (2 rows of 12 tiles at 12x12 px and 1 row of 14x23)

	// ENGINE_TILE_NOX is used to count number of tiles automatically ENGINE_TILE_SIZE is used to size the tiles
	for(int j=0;j<8;j++){		
			for(int i=0;i<ENGINE_TILE_NOX;i++){
					int tileno=(j*ENGINE_TILE_NOX)+i;
					
					// Define regular tiles and high-score half-size tiles
					if(tileno==40||tileno==48||tileno==49||tileno==50||tileno==56||tileno==57||tileno==58){
							t_image[tileno] = makeSprite(s_image, GRect((i%ENGINE_TILE_SIZE)*ENGINE_TILE_SIZE, j*ENGINE_TILE_SIZE, 9, ENGINE_TILE_SIZE));										
					}else{
							t_image[tileno] = makeSprite(s_image, GRect((i%ENGINE_TILE_SIZE)*ENGINE_TILE_SIZE, j*ENGINE_TILE_SIZE, ENGINE_TILE_SIZE, ENGINE_TILE_SIZE));																	
					}
					
					// Define High Score Tiles
					if(j==0){
							if(i<10) t_image[i+64] = makeSprite(s_image, GRect(i*14, 144, 14, 24));				
					}
			}
	}
	
	// Last two Numbers
	t_image[72]=makeSprite(s_image, GRect(8*14, 144, 14, 24));				
	t_image[73]=makeSprite(s_image, GRect(9*14, 144, 14, 24));				
	
	// Letters that have not previously been defined
	t_image[74]=makeSprite(s_image, GRect(9,90,9,18));					// C					
	t_image[75]=makeSprite(s_image, GRect(9,108,9,18));					// R
	t_image[76]=makeSprite(s_image, GRect(27,108,9,18));        // P					
	t_image[77]=makeSprite(s_image, GRect(45,108,9,18));				// G	
	t_image[78]=makeSprite(s_image, GRect(9,126,9,18));					// B
	t_image[79]=makeSprite(s_image, GRect(24,126,15,18));       // M					
	t_image[80]=makeSprite(s_image, GRect(45,126,5,18));				// I	
	t_image[81]=makeSprite(s_image, GRect(50,126,4,18));				// !	
	t_image[82]=makeSprite(s_image, GRect(27,126,9,18));        // V					
	
	// Define transparent images as p_sel and p_walk
	p_sel=gbitmap_create_with_resource(RESOURCE_ID_PNG_TRANSP);
	p_walk=gbitmap_create_as_sub_bitmap(p_sel,GRect(0,18,18,18));
	
	// Prepare for new game
	initGameBoard();
	
	// Prepare Timer
	timer = app_timer_register(delta, (AppTimerCallback) timer_callback, NULL);

}

//---------------------------------------------------------------------------------//
// main_window_unload
//---------------------------------------------------------------------------------//
// Called when window is about to be destroyed
//---------------------------------------------------------------------------------//

static void main_window_unload(Window *window) {
	
	//Cancel timer
	app_timer_cancel(timer);
	
	// Deallocate tiles
	for(int i=0;i<bitmapcnt;i++){
			 gbitmap_destroy(t_image[i]);
	}
		
	// Deallocate bitmap
 	gbitmap_destroy(s_image);
	
	// Deallocate bitmap
 	gbitmap_destroy(p_sel);
	
	// Deallocate bitmap
 	gbitmap_destroy(p_walk);
	
	// Destroy layer
	layer_destroy(s_image_layer);
}

static void init() {
  s_main_window = window_create();
  window_set_click_config_provider(s_main_window, click_config_provider);
	window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
