
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

#define GColorBlackARGB8 ((uint8_t)0b11000000)

static Window *s_main_window;
static Layer *s_image_layer;
static GBitmap *s_image;
static GBitmap *t_image[84];
static unsigned char tilemap[ENGINE_TILEMAPWIDTH*ENGINE_TILEMAPWIDTH];
static unsigned char hiscore[25];
static unsigned char score[5];

static int StartTimer=25;
static unsigned char rlist[4]={1,2,3,4} ;

// Player coordinate and player direction
static int px;
static int py;
static int pdx;
static int pdy;
static int state=1;
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
static int redirect_tile[81] = { 0, 
															  1,  9, 17, 25,  1, 33, 41, 25,			// Vertical (1-4) (5-8)
															  2, 10, 18, 26,  2, 34, 42, 26,			// Horizontal (9-12) (13-16)
																3, 11, 19, 27,  3, 35, 43, 27,      // Bottom Right (17-20) (21-24)
																4, 12, 20, 28,  4, 36, 44, 28,      // Bottom Left (25-28) (29-32)
																5, 13, 21, 29,  5, 37, 45, 29,      // Top Right (33-36) (37-40)
																6, 14, 22, 30,  6, 38, 46, 30,      // Top Left (41-44) (45-48)
																7, 15, 23, 31,  7, 39, 47, 31,      // Crossing Left To Right Empty Vert (49-52) (53-56)
															 54, 51, 59, 61, 54, 52, 60, 61,      // Crossing Left to Right Full Vert (57-60) (61-64)
															  7, 63, 63, 54,  7, 55, 55, 54,      // Crossing Top to Bottom Empty Horizontal (65-68) (69-72)
															 31, 53, 53, 61, 31, 62, 62, 61       // Crossing Top to Bottom Full Horizontal (73-76) (77-80)
															 };

// Garbage collected sprite creation (reference counter only)
static GBitmap* makeSprite(const GBitmap * base_bitmap, GRect sub_rect)
{
	bitmapcnt++;
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "Loop index now %d", bitmapcnt);
	return gbitmap_create_as_sub_bitmap(base_bitmap,sub_rect);
}

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
	
		// Simple float code
		unsigned char ctile=tilemap[(cpy*ENGINE_TILEMAPWIDTH)+cpx];
	
		// Tile delta
		int cpxd=0;
		int cpyd=0;
		unsigned char ntile=0;
	
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
					APP_LOG(APP_LOG_LEVEL_DEBUG, "GAME OVER A %d",ntile);
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
					APP_LOG(APP_LOG_LEVEL_DEBUG, "GAME OVER B %d",ntile);
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
					APP_LOG(APP_LOG_LEVEL_DEBUG, "GAME OVER C %d",ntile);
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
					APP_LOG(APP_LOG_LEVEL_DEBUG, "GAME OVER D %d",ntile);
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
	
	  layer_mark_dirty(s_image_layer);

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
			tilemap[i]=8;
			tilemap[i*ENGINE_TILEMAPWIDTH]=8;
			tilemap[((ENGINE_TILEMAPWIDTH-1)*ENGINE_TILEMAPWIDTH)+i]=8;
			tilemap[(i*ENGINE_TILEMAPWIDTH)+ENGINE_TILEMAPWIDTH-1]=8;
	}
	
	// Move player to center of tile map
	px=ENGINE_PLAYER_STARTX;
	py=ENGINE_PLAYER_STARTY;
	pdx=ENGINE_PLAYER_STARTDX;
	pdy=ENGINE_PLAYER_STARTDY;

	tilemap[(ENGINE_TILEMAPWIDTH*cpy)+cpx]=starttile;	
	tilemap[(ENGINE_TILEMAPWIDTH*(cpy))+(cpx+1)]=68;	
	
	// Randomize mines
	/*
	int tx,ty;
	for(int i=0;i<minecount;i++){
			tx=1+(rand()%22);
			ty=1+(rand()%22);
			tilemap[(tx*24)+ty]=100;
	}
*/
	
}

//---------------------------------------------------------------------------------//
// up_click_handler
//---------------------------------------------------------------------------------//
// Called when select is clicked
//---------------------------------------------------------------------------------//

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
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

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

//---------------------------------------------------------------------------------//
// layer_update_callback
//---------------------------------------------------------------------------------//
// Called when the graphics need to be updated
//---------------------------------------------------------------------------------//

//  graphics_context_set_compositing_mode(ctx, GCompOpSet);
//  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
//  graphics_draw_bitmap_in_rect(ctx, s_bitmap, gbitmap_get_bounds(s_bitmap));
// ENGINE_TILE_SIZE 18
// ENGINE_TILE_NOX 8
//  ENGINE_TILE_NOXH 4

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
	}else if(state>0){
			// Update Tilemap
			int cx;
			int cy;
			int ccx;
			int tileno;

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

						// Draw Player
						if((ccx==px)&&(cy==py)) tileno=8;
						
						// Draw MoveTo Marker
						if((ccx==(px+pdx))&&(cy==(py+pdy))) tileno=16;

						graphics_draw_bitmap_in_rect(ctx, t_image[tileno], GRect(i*ENGINE_TILE_SIZE,j*ENGINE_TILE_SIZE, ENGINE_TILE_SIZE, ENGINE_TILE_SIZE));			
						ccx++;
				}
				cy++;
			}

			// Draw Future Tiles
			for(int i=0;i<3;i++){
					graphics_draw_bitmap_in_rect(ctx, t_image[i+1], GRect(50+(i*22),146, 18, 18));
			}		
			
			graphics_draw_bitmap_in_rect(ctx, t_image[64+(StartTimer/10)], GRect(16,144, 14, 24));
			graphics_draw_bitmap_in_rect(ctx, t_image[64+(StartTimer%10)], GRect(30,144, 14, 24));
	}
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
					t_image[(j*ENGINE_TILE_NOX)+i] = makeSprite(s_image, GRect((i%ENGINE_TILE_SIZE)*ENGINE_TILE_SIZE, j*ENGINE_TILE_SIZE, ENGINE_TILE_SIZE, ENGINE_TILE_SIZE));			
					
					// Define High Score Tiles
					if(j==0){
							if(i<10) t_image[i+64] = makeSprite(s_image, GRect(i*14, 144, 14, 24));				
					}
			}
	}
	
	// Prepare for new game
	state=1;
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
