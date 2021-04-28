/* 046267 Computer Architecture - Spring 2020 - HW #1 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define not_using_share 0
#define  using_share_lsb 1
#define using_share_mid 2
#define LOCAL_TABLES 3
#define LShare 4
#define GShare 5 
#define GLOBAL_HIST 6
#define LOCAL_HIST 7
#define GLOBAL_TABLES 8

typedef struct {
	unsigned btbSize;
	unsigned historySize;
	unsigned tagSize;
	unsigned fsmState;
	int hist_type;
	int tables_type;
	int Shared;

	uint32_t* tag;
	uint32_t* target;
	bool** history;
	bool*** FSM;
	bool* valid;


} BTB;


BTB* Predictor_Table;
SIM_stats* Predictor_Stats;

void reverse(bool arr[],int len){

	int i=0;
	int j=len-1;
	while(i<=j){
		bool tmp = arr[i];
		arr[i] = arr[j];
		arr[j] = tmp;
		i++;
		j--;
	}
}

unsigned int NumBits(unsigned int n)
{	
	//printf("\nIn Numbits: n= %d\n",n); //debug
   unsigned int count = 0;
   while (n)
   {
        count++;
        n >>= 1;
    }
    return count;
}

void decimalToBinary(int n ,bool bits[],int len)
{	
	//int len_bits = NumBits(n);
	//printf("num of bits is %d \n",len_bits);
	for(int k=0 ; k<len ; k++){
		bits[k] = (n & ( 1 << k )) >> k;
	}
    	

	reverse(bits,len);
}
int BinaryToDecimal(bool n[] , int start , int end) {
    int dec = 0;
    for(int i=end ; i>=start ; i--) {
        dec += n[i]*pow(2, end-i);
    }
    return dec;
}

/*void decimalToBinary(int n ,bool bits[])
{	
	int len = NumBits(n);
	for(int k=len ; k>=0 ; k--)
    	bits[k] = (n & ( 1 << k )) >> k;
}*/

int stats_init(unsigned historySize,bool isGlobalHist, bool isGlobalTable, int Shared , int btbSize , int tagSize){

	Predictor_Stats = (SIM_stats*)malloc(sizeof(SIM_stats));
	if(Predictor_Stats == NULL){
		printf("memory allocation failed!");
		return -1;
	}
	Predictor_Stats->flush_num = 0;
	Predictor_Stats->br_num = 0;
	//CHECK IF SIZE OF VALID IS 1 OR 30
	if(!isGlobalHist&&!isGlobalTable)
		Predictor_Stats->size = btbSize*(tagSize + 30 + historySize + pow(2,historySize +1) );
	else if(!isGlobalHist&&isGlobalTable)
		Predictor_Stats->size = btbSize*(tagSize + 30 + historySize ) + pow(2,historySize +1);
	else if(isGlobalHist&&!isGlobalTable)
		Predictor_Stats->size = btbSize*(tagSize + 30 + pow(2,historySize +1) ) + historySize ;
	//if(isGlobalHist&&isGlobalTable)
	else
		Predictor_Stats->size = historySize  + pow(2,historySize +1) + (tagSize+30)*btbSize ;
	

	return 0;

}

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){
	

	int res = stats_init(historySize ,isGlobalHist, isGlobalTable,Shared ,btbSize ,tagSize);
	if(res == -1){
		return -1;
	}
	Predictor_Table = (BTB*)malloc(sizeof(BTB));
	if(Predictor_Table == NULL){
		free(Predictor_Stats);
		printf("memory allocation failed!");
		return -1;
	}
	Predictor_Table->valid = (bool*)malloc(sizeof(bool)*btbSize);
	if(Predictor_Table->valid == NULL){
		free(Predictor_Stats);
		free(Predictor_Table);
		printf("memory allocation failed!");
		return -1;		
	}
	for(unsigned i=0;i<btbSize ;i++) Predictor_Table->valid[i] = false;
	Predictor_Table->btbSize = btbSize;
	Predictor_Table->historySize = historySize;
	Predictor_Table->tagSize = tagSize;
	Predictor_Table->fsmState = fsmState;
	Predictor_Table->Shared = Shared;
	Predictor_Table->tag = (uint32_t*)malloc(sizeof(uint32_t)*btbSize);
	if(Predictor_Table->tag == NULL){
		free(Predictor_Table);
		free(Predictor_Stats);
		printf("memory allocation failed!");
		return -1;
	}
	Predictor_Table->target = (uint32_t*)malloc(sizeof(uint32_t)*btbSize);
	if(Predictor_Table->tag == NULL){
		free(Predictor_Stats);
		free(Predictor_Table->tag);
		free(Predictor_Table);
		printf("memory allocation failed!");
		return -1;
	}
	int COUNTERS_NUM_1 , COUNTERS_NUM_2 ;
	int HISTS_NUM;
	if(isGlobalTable){
		Predictor_Table->tables_type = GLOBAL_TABLES;
		COUNTERS_NUM_2 = 1;
	}
	else{
		COUNTERS_NUM_2 = btbSize;
		Predictor_Table->tables_type = LOCAL_TABLES;
		//COUNTERS_NUM_2 = btbSize;
	}
	COUNTERS_NUM_1= pow(2,historySize);
	Predictor_Table->FSM = (bool***)malloc(sizeof(bool**)*(COUNTERS_NUM_2));
	if(Predictor_Table->FSM == NULL){

		printf("memory allocation failed!");
		free(Predictor_Table);
		return -1;
	}
	for(int i=0 ; i<COUNTERS_NUM_2 ; i++){
		Predictor_Table->FSM[i] = (bool**)malloc(sizeof(bool*)*COUNTERS_NUM_1);
	}
	for(unsigned i=0 ;i<COUNTERS_NUM_2 ;i++){
		for(unsigned j=0 ; j<COUNTERS_NUM_1 ; j++){

			Predictor_Table->FSM[i][j] = (bool*)malloc(sizeof(bool)*2);
			decimalToBinary(fsmState,Predictor_Table->FSM[i][j],2);
		}
		
		/*if(Predictor_Table->FSM[i] == NULL){
			printf("memory allocation failed!");

			for(unsigned j=0 ; j<i ; j++)
				free(Predictor_Table->FSM[j]);
			free(Predictor_Table->FSM);
			free(Predictor_Table);
			return -1;
		}*/
		//printf("initial fsm is %d%d\n",Predictor_Table->FSM[i][0],Predictor_Table->FSM[i][1]);
	}
	//history
	if(isGlobalHist){
		HISTS_NUM = 1;
		Predictor_Table->hist_type = GLOBAL_HIST;
	}
	else{
		HISTS_NUM = btbSize;
		Predictor_Table->hist_type = LOCAL_HIST;
	}
	Predictor_Table->history =(bool**)malloc(sizeof(bool*)*(HISTS_NUM));
	/*if(Predictor_Table->history == NULL){
			printf("memory allocation failed!");

			for(unsigned j=0 ; j<COUNTERS_NUM ; j++)
				free(Predictor_Table->FSM[j]);
			free(Predictor_Table->FSM);
			free(Predictor_Table);
			return -1;
	}*/
	for(unsigned i=0;i<HISTS_NUM;i++){
		Predictor_Table->history[i] = (bool*)malloc(sizeof(bool)*historySize);
			//if(Predictor_Table->history[i] == NULL){
			/*printf("memory allocation failed!");

			for(unsigned j=0 ; j<COUNTERS_NUM ; j++)
				free(Predictor_Table->FSM[j]);
			free(Predictor_Table->FSM);

			for(unsigned j=0 ; j<i ; j++)
				free(Predictor_Table->history[j]);
			free(Predictor_Table->history);
			free(Predictor_Table);
			return -1;
		}*/

}
	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){

	bool PC_addrs[32];
	for(int i=0;i<32;i++) PC_addrs[i] = false;
	bool found = true;
	decimalToBinary(pc,PC_addrs,32);
	//printf("pc in binary is:\n");
	//for(int i=0;i<32;i++)printf("%d",PC_addrs[i]);
	//printf("\n"); //debug
	int fsm_index;
	int fsm_ex_index;
	int end = log2(Predictor_Table->btbSize)+1;
	int index = BinaryToDecimal(PC_addrs,32-end-2,29);
	int history_index ;
	if(Predictor_Table->tables_type == LOCAL_TABLES){
		fsm_ex_index = index;
	}
	else fsm_ex_index = 0;
	if(Predictor_Table->hist_type == GLOBAL_HIST) history_index = 0;
	else history_index = index;
	if(Predictor_Table->valid[index]){

		//printf("this pc is valid : %d and index is %d \n",pc,index); //debug
		for(unsigned i=32-end , j=Predictor_Table->tagSize-1 ; (i< 32-end - Predictor_Table->tagSize)&&j>=0 ; i-- , j--){

			if(PC_addrs[i] != Predictor_Table->tag[j]){
				found = false;
				break;
			}
		}

	}
	else{
		*dst  = pc + 4;
		return false;
	}
	if(found){
		
		//taken or not taken
		//find fsm index
		//printf("is found , dst is : %d\n",*dst); //debug
		if(Predictor_Table->Shared == using_share_lsb){
			fsm_index = BinaryToDecimal(PC_addrs,29-Predictor_Table->historySize+1,29) ^ BinaryToDecimal(Predictor_Table->history[history_index] , 0 , Predictor_Table->historySize -1) ;
			if(Predictor_Table->FSM[fsm_ex_index][fsm_index][0]){

				*dst = Predictor_Table->target[index];
				return true;
			}
			else{
				*dst = pc + 4;
				return false;
			}
		}
		else if(Predictor_Table->Shared == using_share_mid){
			fsm_index = BinaryToDecimal(PC_addrs,16-Predictor_Table->historySize+1,16) ^ BinaryToDecimal(Predictor_Table->history[history_index],0, Predictor_Table->historySize -1);
			if(Predictor_Table->FSM[fsm_ex_index][fsm_index][0]){
				*dst = Predictor_Table->target[index];
				return true;
			}
			else{
				*dst = pc +4;
				return false;
			}
			
		}
		else{
			fsm_index = BinaryToDecimal(Predictor_Table->history[index],0,Predictor_Table->historySize);
			//printf("fsm state is : %d%d",Predictor_Table->FSM[fsm_ex_index][fsm_index][0],Predictor_Table->FSM[fsm_ex_index][fsm_index][1]);
			if(Predictor_Table->FSM[fsm_ex_index][fsm_index][0]){
				*dst = Predictor_Table->target[index];
				return true;				
			}
			else{
				*dst = pc + 4;
				return false;
			}
		}
	}
	//else
	*dst = pc + 4 ;
	return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){


	Predictor_Stats->br_num++;
	bool PC_addrs[32];
	bool found = true;
	decimalToBinary(pc,PC_addrs,32);
	int end = log2(Predictor_Table->btbSize)+1;
	int index = BinaryToDecimal(PC_addrs,32-end-1,29);
	int fsm_index , fsm_ex_index;
	int history_index ;
	if(Predictor_Table->tables_type == LOCAL_TABLES){
		fsm_ex_index = index;
	}
	else fsm_ex_index = 0;
	if(Predictor_Table->hist_type == GLOBAL_HIST) history_index = 0;
	else history_index = index;
	
	if(Predictor_Table->Shared == using_share_lsb){
		fsm_index = BinaryToDecimal(PC_addrs,29-Predictor_Table->historySize+1,29) ^ BinaryToDecimal(Predictor_Table->history[history_index],0,Predictor_Table->historySize) ;
		
	}
	else if(Predictor_Table->Shared == using_share_mid){
		fsm_index = BinaryToDecimal(PC_addrs,16-Predictor_Table->historySize+1,16) ^ BinaryToDecimal(Predictor_Table->history[history_index],0,Predictor_Table->historySize);
		
	}
	else{
		fsm_index = BinaryToDecimal(Predictor_Table->history[index],0,Predictor_Table->historySize);
		
	}

	if(Predictor_Table->valid[index]){

		for(unsigned i=29-end+1 , j=Predictor_Table->tagSize-1 ; (i< 29-end +1 - Predictor_Table->tagSize +1)&&j>=0 ; i-- , j--){
			

			//printf("%u %u\n",PC_addrs[i],Predictor_Table->tag[j]); //debug
			if(PC_addrs[i] != Predictor_Table->tag[j]){
				found = false;
				break;
			}
		}

	}
	if(!Predictor_Table->valid[index] || !found){
		Predictor_Table->tag[index] = BinaryToDecimal(PC_addrs,29-end+1-Predictor_Table->tagSize+1,29-end+1 );
		Predictor_Table->target[index] = targetPc;
		//printf("being add to table , pc is :%d with targetpc : %d\n",pc,targetPc);
		//printf("%d\n",targetPc); //debug
		if(Predictor_Table->hist_type != GLOBAL_HIST){
			for(unsigned i=0 ; i<Predictor_Table->historySize;i++)
				Predictor_Table->history[index][i] = 0;
		}
		if(Predictor_Table->tables_type != GLOBAL_TABLES){

		int NewState ;
		if(taken) NewState = (Predictor_Table->fsmState +  1 ) >=3 ? 3 : (Predictor_Table->fsmState +  1 );
		else NewState = (Predictor_Table->fsmState - 1) <= 0 ? 0 : (Predictor_Table->fsmState - 1);
		decimalToBinary(NewState, Predictor_Table->FSM[fsm_ex_index][fsm_index],2);
		//printf("first state is %d , %d%d",NewState,Predictor_Table->FSM[fsm_ex_index][fsm_index][0],Predictor_Table->FSM[fsm_ex_index][fsm_index][1]);
	}
		Predictor_Table->valid[index] = true;
	}

	if(found){
		if((taken&&(pred_dst!=targetPc))||(!taken&&(pred_dst!=pc+4))){
			Predictor_Stats->flush_num++;
		}
		Predictor_Table->target[index] = targetPc;
		for(unsigned i = 0;i<Predictor_Table->historySize -1 ;i++){
			Predictor_Table->history[history_index][i] = Predictor_Table->history[history_index][i+1];
		}
		Predictor_Table->history[history_index][Predictor_Table->historySize-1] = taken;
		int NewState ;
		printf("fsm_index is %d and fsm_ex_index is %d num is %d%d \n",fsm_index,fsm_ex_index,Predictor_Table->FSM[fsm_ex_index][fsm_index][0],Predictor_Table->FSM[fsm_ex_index][fsm_index][1]);
		if(taken) NewState = (BinaryToDecimal(Predictor_Table->FSM[fsm_ex_index][fsm_index],0,1) +  1 ) >=3 ? 3 : (BinaryToDecimal(Predictor_Table->FSM[fsm_ex_index][fsm_index],0,1) +  1 );
		else NewState = (BinaryToDecimal(Predictor_Table->FSM[fsm_ex_index][fsm_index],0,1) - 1) <= 0 ? 0 : (BinaryToDecimal(Predictor_Table->FSM[fsm_ex_index][fsm_index],0,1) - 1);
		//printf("new state is %d prev state is %d%d\n",NewState,Predictor_Table->FSM[fsm_ex_index][fsm_index][0],Predictor_Table->FSM[fsm_ex_index][fsm_index][1]);
		decimalToBinary(NewState, Predictor_Table->FSM[fsm_ex_index][fsm_index],2);
		//printf("new state in binary is %d%d\n",Predictor_Table->FSM[fsm_ex_index][fsm_index][0],Predictor_Table->FSM[fsm_ex_index][fsm_index][1]);


	}		
	return;
}

void BP_GetStats(SIM_stats *curStats){

	curStats->flush_num = Predictor_Stats->flush_num;
	curStats->br_num = Predictor_Stats->br_num;
	curStats->size = Predictor_Stats->size;
	return;
}

