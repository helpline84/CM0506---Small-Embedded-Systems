#include <mbed.h>
#include <MMA7455.h>
#include <LM75B.h>
#include <display.h>

//Declare output object for LED1
DigitalOut led1(LED1);

// Initialise Joystick   
typedef enum {JLT = 0, JRT, JUP, JDN, JCR} btnId_t;
static DigitalIn jsBtns[] = {P5_0, P5_4, P5_2, P5_1, P5_3}; // LFT, RGHT, UP, DWN, CTR
bool jsPrsdAndRlsd(btnId_t b);

//Input object for the potentiometer
AnalogIn pot(p15);
float potVal = 0.0;
  
//Object to manage the accelerometer
MMA7455 acc(P0_27, P0_28);
bool accInit(MMA7455& acc); //prototype of init routine
int32_t accVal[3];

//Object to manage temperature sensor
LM75B lm75b(P0_27, P0_28, LM75B::ADDRESS_1);
float tempVal = 0.0;

Display *screen = Display::theDisplay();
    //This is how you call a static method of class Display
    //Returns a pointer to an object that manages the display screen 

//Timer interrupt and handler
void timerHandler(); //prototype of handler function
int tickCt = 0;

//Game varialbes
bool gameActive = 0;
int ballsRemaining = 0;
int score = 0;
int scoringRate = 0;
int returns = 0;
bool gameOver = 0;

//Game functions
void loseLife();
void calculateScore();

//Additional functions
int randomRange(int, int);
void initialiseDevices();
int initialiseGame();
void waitForStart();
void outOfBounds();

//Drawing coordinates for ball
int ballX, ballY, ballNewX, ballNewY;
//Additional ball variables
int ballSpeed;
int ballDirectionX;
int ballDirectionY;
bool ballInPlay;

//Drawing coordinates for bat
int batX, batY, batNewX;		

//Decalres a struct of type bat
typedef struct Bat {
	int batX, batY; //position coordinates
	int batNewX; //velocity coordinate
	int width, height; //dimensions
} bat_t;

	bat_t bat;

//bat functions
void newBat();
bat_t renderBat(bat_t);
bat_t updateBat(bat_t);
//Decalres a struct of type obstacle
typedef struct Obstacle{
	int obstacleX, obstacleY;
	int width, height;	
} obstacle_t;

	obstacle_t obstacle;


//Declares a struct of type ball
typedef struct Ball {
		int ballX, ballY;	//position coordinates
		int ballNewX, ballNewY; //velocity coordinates
		int ballRadius;		
		int ballSpeed; //ball speed multiplier
		int ballDirectionX, ballDirectionY; //initial direction values
		} ball_t;	
		
		ball_t ball;	

//Ball functions
void newBall();
void assignBallDirection(ball_t);
ball_t renderBall(ball_t);
void calculateBallSpeed(ball_t);
ball_t updateBall(ball_t, bat_t, obstacle_t);
int collision(ball_t, bat_t, obstacle_t);		

//functions for obstacles
void newObstacle();
obstacle_t renderObstacle(obstacle_t);
int obstacleCollision(ball_t, obstacle_t);
ball_t directionInvert(ball_t);

int main() {
	while(true){
		
		initialiseDevices();  
		ballsRemaining = initialiseGame();
		newBat();
	
		while (ballsRemaining > 0) {      
			//potVal = pot.read();   		  
		
			waitForStart();	    

			ball = renderBall(ball);
			ball = updateBall(ball, bat, obstacle);
		
		
			while (ballInPlay) {	     			
				ball = renderBall(ball);
				ball = updateBall(ball,bat,obstacle);
				obstacle = renderObstacle(obstacle);
				bat = renderBat(bat);
				bat = updateBat(bat);		
				wait(0.005); //5 milliseconds
			}
		
			wait(0.005); //5 milliseconds
		}
}
}

bool accInit(MMA7455& acc) {
  bool result = true;
  if (!acc.setMode(MMA7455::ModeMeasurement)) {
    // screen->printf("Unable to set mode for MMA7455!\n");
    result = false;
  }
  if (!acc.calibrate()) {
    // screen->printf("Failed to calibrate MMA7455!\n");
    result = false;
  }
  // screen->printf("MMA7455 initialised\n");
  return result;
}

//Definition of timer interrupt handler
void timerHandler() {
  tickCt++;
  led1 = !led1; //every tick, toggle led1
}

/* Definition of Joystick press capture function
 * b is one of JLEFT, JRIGHT, JUP, JDOWN - from enum, 0, 1, 2, 3
 * Returns true if this Joystick pressed then released, false otherwise.
 *
 * If the value of the button's pin is 0 then the button is being pressed,
 * just remember this in savedState.
 * If the value of the button's pin is 1 then the button is released, so
 * if the savedState of the button is 0, then the result is true, otherwise
 * the result is false. */
bool jsPrsdAndRlsd(btnId_t b) {
	bool result = false;
	uint32_t state;
	static uint32_t savedState[4] = {1,1,1,1};
        //initially all 1s: nothing pressed
	state = jsBtns[b].read();
  if ((savedState[b] == 0) && (state == 1)) {
		result = true;
	}
	savedState[b] = state;
	return result;
}

void initialiseDevices(){
	
	// Initialise the display
  screen->fillScreen(WHITE);
  screen->setTextColor(BLACK, WHITE);

  // Initialise accelerometer and temperature sensor
  screen->setCursor(2,82);
  if (accInit(acc)) {
		//screen->printf("Accelerometer initialised");
	} else {
    //screen->printf("Could not initialise accelerometer");
  }
  lm75b.open();
  
	
	
  //Initialise ticker and install interrupt handler
  Ticker ticktock;
  ticktock.attach(&timerHandler, 1); 
  
	
}

int initialiseGame(){
	
//reset score
score = 0;
scoringRate = 1;
	
//set ballsremaining
ballsRemaining = 5;
	
screen->setCursor(2,2);
screen->printf("Balls Remaining: %d", ballsRemaining);
		
screen->setCursor(400,2);
screen->printf("Score: %d", score );

screen->fillRect(0, 10, 480, 1, BLACK);
	
return ballsRemaining;

}

void waitForStart(){
		//Polls joystick centre
	  while(!ballInPlay) {
			bat = renderBat(bat);
			bat = updateBat(bat);
			//Ball spawns if the centre joystick button is pressed
			if (jsPrsdAndRlsd(JCR)) {				
				newBall();
				//creates a new obstacle each time a new ball is summoned
				newObstacle();
				returns = 0;
				scoringRate = 1;
				ballInPlay = 1;			
			}
		}
}

//Creates a new ball, moving in at a random speed and in a random direction
//ballSpeed is the rate at which the ball moves
//ballDirection in which the ball moves 
//1 = up, 2 = down(ballY), 3 = left, 4 = right(ballX)
//Initial ballspeed and balldirection are random		
		
void newBall(){	
	
		new struct Ball;
		
		ball.ballX = randomRange(10, 460); 
		ball.ballY = 50;
	  ball.ballNewX = 1;
		ball.ballNewY = -1;
		ball.ballSpeed = randomRange(1, 5);		
		ball.ballRadius = 5;		
		ball.ballDirectionX = randomRange(3, 4);
		ball.ballDirectionY = randomRange(1, 2);
		//assigns the balls starting direction
		assignBallDirection(ball);		
		
}

//Generate and return a random integer value within the specified range
int randomRange(int from, int to)
{
    int range = to-from;
    return from  + rand()%(range+1);
}

//Assign the initial ball velocity
void assignBallDirection(ball_t ball){	
		
		//Assign Horizontal Direction
		if (ball.ballDirectionX == 3){
			//Ball moves Left on the X axis
			ball.ballNewX = -1;
			ball.ballDirectionX = 0;
		}
		else if (ball.ballDirectionX == 4){
			//Ball moves Right on the X axis
			ball.ballNewX = 1;
			ball.ballDirectionX = 0;
		}
		
		//Assign Vertical Direction
		if (ball.ballDirectionY == 1){
			//Ball moves Up on the Y axis
			ball.ballNewY = -1;
			ball.ballDirectionY = 0;
		}
		else if (ball.ballDirectionY == 2){
			//Ball moves Down on the Y axis
			ball.ballNewY = 1;
			ball.ballDirectionY = 0;
		}		
		
		
}

ball_t renderBall (ball_t ball) {
	
		
		
		//updates the ball position on the screen
	  	
		screen->fillCircle(ball.ballX, ball.ballY, ball.ballRadius, WHITE);
	
		calculateBallSpeed(ball);	
	 	
		ball.ballX += ball.ballNewX;
		ball.ballY += ball.ballNewY;			
	
		screen->fillCircle(ball.ballX, ball.ballY, ball.ballRadius, BLUE);
	
		return ball;
}

void calculateBallSpeed(ball_t ball){			
	
		ball.ballNewX = ball.ballNewX * ball.ballSpeed;
		ball.ballNewY = ball.ballNewY * ball.ballSpeed;			
	
}

ball_t updateBall(ball_t ball, bat_t bat, obstacle_t obstacle){
		
	
		/* BALL COLLISIONS
		//Checks to see if the ball has "collided" with the court boundaries
		//The left boundary is X = 0, the right boundary is X = 480
		//The top boundary is Y = 0, the bottom boundary is Y = 270
		//If the ball collides with the left, right or top boundaries it bounces
		//If the ball collides with the bat it also bounces
		//If it passes the bottom boundary, a new ball is made and a life is lost
		*/
		
		//Left Boundary Check
		if (ball.ballX <= 0){
			ball.ballNewX = 1;
			
		}
		
		//Right Boundary Check
		if (ball.ballX >= 480){
			ball.ballNewX = -1;
			
		}
		
		//Top Boundary Check
		if (ball.ballY <= 25){
			ball.ballNewY = 1;
			calculateScore();
		}
		
		//Collision check
		if (collision(ball, bat, obstacle)){
				ball = directionInvert(ball);				
		}
				
		//Bottom Boundary Check
		if (ball.ballY >= 285){
			outOfBounds();					
		}
				
		return ball;
		
}


//Initialises a new bat at the bottom of the screen
void newBat (){	
	
	new struct Bat;
	
	bat.batX = 215;
	bat.batY = 260;
	bat.batNewX = 2;
	bat.width = 40;
	bat.height = 4;
	
}

bat_t renderBat(bat_t bat){
			
		//draws the bat in its new position
		screen->fillRect(bat.batX, bat.batY, bat.width, bat.height, WHITE);
		bat.batX += bat.batNewX;
		screen->fillRect(bat.batX, bat.batY, bat.width, bat.height, BLACK);
	
		return bat;
	
}

bat_t updateBat(bat_t bat){
		
		//reads movement from the accelerometer
		acc.read(accVal[0], accVal[1], accVal[2]);
	
		/*BAT COLLISIONS & MOVEMENT
		//If the bat hits the left boundary, it reverses direction
		//If the bat hits the right boundary, it reveres direction
		*/	
		if (accVal[0] >= 20) {
			//Right Boundary Check
			if (bat.batX >= 440){
				bat.batNewX = -2;
			}																	
			else{													
				bat.batNewX = 2;  
			}																		
		} 
		else if (accVal[0] <= -20) {
			//Left Boundary Check
			if(bat.batX <= 0){
				bat.batNewX = 2;
			}
			else {
				bat.batNewX = -2; 
			}
		}
		else {
			bat.batNewX = 0;
		}
	
		return bat;
}

int collision(ball_t ball, bat_t bat, obstacle_t obstacle){
	
	int collision = 0;
	
	//Bat Collision
	//checks to see if the ball is at the same height on the y axis as the bat
	if ((ball.ballY + ball.ballRadius) >= (bat.batY + bat.height)){
		//checks that the ball is at the same position on the x axis as the bat
		if((ball.ballX >= bat.batX) && (ball.ballX <= bat.batX + bat.width)){
			//If both are true a collision has occured
			collision = 1;				
		}
	}
	
	//Obstacle Collision
	//checks to see if the ball is at the same height on the y axis as the obstacle
	if ((ball.ballY + ball.ballRadius) == (obstacle.obstacleY + obstacle.height)){
		//checks that the ball is at the same position on the x axis as the obstacle
		if((ball.ballX >= obstacle.obstacleX) && (ball.ballX <= obstacle.obstacleX + obstacle.width)){
			//If both are true a collision has occured
			collision = 1;			
		}
	}	
		
	return collision;	
	
}

void newObstacle(){
		//clear old obstacle
		screen->fillRect(obstacle.obstacleX, obstacle.obstacleY, obstacle.width, obstacle.height, WHITE);
	
		//create a new obstacle
		new struct Obstacle;
	
		obstacle.obstacleX = randomRange(0, 440) ;
		obstacle.obstacleY = randomRange(20, 130) ;
		obstacle.width = randomRange(40, 200);
		obstacle.height = 2;		
		
}

obstacle_t renderObstacle(obstacle_t obstacle){
		//Draws the new obstacle
		screen->fillRect(obstacle.obstacleX, obstacle.obstacleY, obstacle.width, obstacle.height, GREEN);
	
		return obstacle;
}

ball_t directionInvert(ball_t ball){
		//if the ball is moving up it is changed to move down
		if(ball.ballNewY == -1){
			ball.ballNewY = 1;
		}
		//if the ball is moving down it is changed to move up
		if(ball.ballNewY == 1){
			ball.ballNewY = -1;
		}
		
		return ball;
}

void calculateScore(){
		returns += 1;
		
		if(returns/5 == 0){
			scoringRate += 1;
		}
		
		score = score + (1 * scoringRate);
		screen->setCursor(400,2);
		screen->printf("Score: %d", score );
}

void outOfBounds(){
			//Reduces the number of balls remaining
			ballInPlay = 0;
			ballsRemaining -= 1;
	
			//updates the display
		  screen->setCursor(2,2);
			screen->printf("Balls Remaining: %d", ballsRemaining);
			
}

//Removes a life if the ball goes out through the bottom of the court
void loseLife(){
		//Removes a life from the player
		ballsRemaining = ballsRemaining - 1;
		//If the player runs out of lives, the game ends
		if(ballsRemaining <= 0){
			gameOver = 1;
			gameActive = 0;
		}
}
