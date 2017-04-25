 
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>

#define GREEN_LED BIT6

AbRect rect10 = {abRectGetBounds, abRectCheck, {10,10}};

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2-2, screenHeight/2 - 20}
};



Layer layer1 = {		/**< Layer with an orange circle */
  (AbShape *)&circle8,
  {screenWidth-10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  0,
  0
};

Layer layer2 = {		/**< Layer with an orange circle */
  (AbShape *)&circle2,
  {27, screenHeight/2+18}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_YELLOW,
  0,
  &layer1
};

Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2-15},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLUE,
  1,
  &layer2
};

Layer layer0 = {		/**< Layer with a red square */
  (AbShape *)&rect10,
  {16, screenHeight/2+18}, /**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_RED,
  1,
  &fieldLayer,
};

typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  u_char active;
  struct MovLayer_s *next;
} MovLayer;

MovLayer bullet = { &layer2, {7,0}, 0, 0 };
MovLayer enemy1 = { &layer1, {-1,-4}, 0 ,&bullet}; 
MovLayer player = { &layer0, {0,-4}, 0, &enemy1};

void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;
  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
        Region bounds;
        layerGetBounds(movLayer->layer, &bounds);
        lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
            bounds.botRight.axes[0], bounds.botRight.axes[1]);
        for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
            for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
                Vec2 pixelPos = {col, row};
                u_int color = bgColor;
                Layer *probeLayer;
                for (probeLayer = layers; probeLayer;probeLayer = probeLayer->next) { /* probe all layers, in order */
                    if(probeLayer->active){
                        if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
                        color = probeLayer->color;
                        break;
                        }
                    } /* if probe check */
                } // for checking all layers at col, row
                lcd_writeColor(color); 
            } // for col
        } // for row
    
  } // for moving layer being updated
}	  

void bulletAdvance(MovLayer *ml, MovLayer *enemy, Region *fence){
  Vec2 newPos;
  Region shapeBoundary;
  vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
  abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
  if(enemy->active){
      Region enemyBoundary;
      layerGetBounds(enemy->layer, &enemyBoundary);
        if ((shapeBoundary.topLeft.axes[0] < enemyBoundary.botRight.axes[0]) &&
            (shapeBoundary.botRight.axes[0] > enemyBoundary.topLeft.axes[0]) &&
            (shapeBoundary.topLeft.axes[1] < enemyBoundary.botRight.axes[1]) &&
            (shapeBoundary.botRight.axes[1] > enemyBoundary.topLeft.axes[1])) {
                ml->active=0;
                ml->layer->active=0;
                enemy->active=0;
                enemy->layer->active=0;
        }
      }
if ((shapeBoundary.botRight.axes[0] > fence->botRight.axes[0])) {
          ml->active=0;
          ml->layer->active=0;
      }	/**< if outside of fence */
        ml->layer->posNext = newPos; 
}


void playerJump(MovLayer *ml, Region *fence){
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
  abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
  for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]+10) ||
      (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
          int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
          newPos.axes[axis] += (2*velocity);
          if(shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]){ 
            ml->active=0;
            newPos.axes[1]=fence->botRight.axes[1]-11;
          }
      }	/**< if outside of fence */
  }
        ml->layer->posNext = newPos; 
}

void enemy1Advance(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  Region shapeBoundary;
  vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
  abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
  if ((shapeBoundary.topLeft.axes[0] < fence->topLeft.axes[0])) {
      newPos.axes[0] = screenWidth-18;
      ml->active=0;
      ml->layer->active=0;
  }
  if ((shapeBoundary.topLeft.axes[1] < fence->topLeft.axes[1]) ||
  (shapeBoundary.botRight.axes[1] >fence->botRight.axes[1]) ) {
      int velocity = ml->velocity.axes[1] = -ml->velocity.axes[1];
      newPos.axes[1] += (2*velocity);
  }/**< if outside of fence */
  ml->layer->posNext = newPos;  
}


u_int bgColor = COLOR_BLUE;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */


/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15);

  shapeInit();
  clearScreen(COLOR_GREEN);
  layerInit(&layer0);
  layerDraw(&layer0);


  layerGetBounds(&fieldLayer, &fieldFence);

  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */
  for(;;) { 
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&player, &layer0);
  }
}

/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static char stop=0;
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  unsigned int sw=p2sw_read();
  if (sw==14 && player.active==0){
      player.active=1;
    }
    
  if(sw==7 && enemy1.active==0){
      enemy1.layer->posNext.axes[0]=screenWidth-10;
      enemy1.active=1;
      enemy1.layer->active=1;
    }

  if(sw==13 && bullet.active==0){
      bullet.layer->pos=player.layer->posNext;
      bullet.layer->posNext=player.layer->posNext;
      bullet.active=1;
      bullet.layer->active=1;
}
    
  if (count == 15) {
    if(player.active){
          playerJump(&player, &fieldFence);
          redrawScreen = 1;
    }
    if(enemy1.active){
        enemy1Advance(&enemy1,&fieldFence);
        redrawScreen = 1;
    }
    
    if(bullet.active){
        bulletAdvance(&bullet,&enemy1,&fieldFence);
        redrawScreen=1;
    }
    
    count = 0;
  }
  
  count++;
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
