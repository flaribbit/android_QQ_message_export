#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <wincrypt.h>
#include "key.h"
#include "sqlite3.h"

typedef struct{
	ULONG i[2];
	ULONG buf[4];
	unsigned char in[64];
	unsigned char digest[16];
} MD5_CTX;

int GetMd5(unsigned char *data,int datalen,unsigned char *md5){
	char t[]="0123456789ABCDEF";
	HMODULE hCryptdll = LoadLibraryA("cryptdll.dll");
	void (WINAPI *MD5Init)(MD5_CTX*) = (void (WINAPI *)(MD5_CTX *))GetProcAddress(hCryptdll, "MD5Init");
	void (WINAPI *MD5Update)(MD5_CTX*, unsigned char*, unsigned int) = (void (WINAPI *)(MD5_CTX*, unsigned char*, unsigned int))GetProcAddress(hCryptdll, "MD5Update");
	void (WINAPI *MD5Final)(MD5_CTX*) = (void (WINAPI *)(MD5_CTX*))GetProcAddress(hCryptdll, "MD5Final");
	MD5_CTX ctx;
	MD5Init(&ctx);
	MD5Update(&ctx,data,datalen);
	MD5Final(&ctx);
	for(int i=0;i<16;i++){
		md5[2*i]=t[ctx.digest[i]>>4];
		md5[2*i+1]=t[ctx.digest[i]&0xf];
	}
	md5[32]=0;
	FreeLibrary(hCryptdll);
	return 0;
}

void GetDate(time_t timestamp,char *dt){
	struct tm *date=gmtime(&timestamp);
	sprintf(dt,"%04d-%02d-%02d %02d:%02d:%02d",date->tm_year+1900,date->tm_mon+1,date->tm_mday,date->tm_hour,date->tm_min,date->tm_sec);
}

int GetBlob(sqlite3 *db,char *name,char *table,char *col,int row,char *buf,int *bufsize){
	sqlite3_blob *blob;
	//尝试访问blob
	int rc=sqlite3_blob_open(db,name,table,col,row,0,&blob);
	if(rc!=SQLITE_OK){
		printf("Failed to open BLOB: %s\n",sqlite3_errmsg(db));
		return -1;
	}
	//获取大小
	*bufsize=sqlite3_blob_bytes(blob);
	//读取数据
	sqlite3_blob_read(blob,buf,*bufsize,0);
	sqlite3_blob_close(blob);
	return 0;
}

int ByteDecode(unsigned char *data,int datalen,char *key,int keylen){
	for(int i=0;i<datalen;i++){
		data[i]^=key[i%keylen];
	}
	return 0;
}

int TextDecode(char *s,int len,const char *key,int keylen){
    for(int i=0,j=0;i<len;i++,j++){
        if((s[i]&0xE0)==0xC0)i+=1;
        else if((s[i]&0xF0)==0xE0)i+=2;
        s[i]^=key[j%keylen];
    }
	return 0;
}

int GetCounts(void*a,int c,char**v,char**n){
	*(int*)a=atoi(*v);
	return 0;
}

int GetMsg(sqlite3* db,char *table,char *key,int keylen,char *output){
	//合并指令
	char buf[1024];
	sprintf(buf,"select count(_id) from %s",table);
	//计算长度
	//select count(_id) from table
	int rows;
	char *err;
	int rc=sqlite3_exec(db,buf,GetCounts,&rows,&err);
	if(rc!=SQLITE_OK){
		printf("SQL error: %s\n",err);
		return -1;
	}
	sqlite3_stmt* stmt;
	//合并指令
	strcpy(buf,"select time,msgData from ");
	strcat(buf,table);
	//准备获取数据
	sqlite3_prepare(db,buf,strlen(buf),&stmt,0);
	char *blob;
	int bloblen;
	int time;
	//获取数据
	FILE *fp=fopen(output,"wb");
	fprintf(fp,"\xEF\xBB\xBF");
	while(1){
		rc=sqlite3_step(stmt);
		if(rc==SQLITE_ROW){
			time=sqlite3_column_int(stmt,0);
			blob=(char*)sqlite3_column_blob(stmt,1);
			bloblen=sqlite3_column_bytes(stmt,1);
			ByteDecode((unsigned char*)blob,bloblen,key,keylen);
			fwrite(blob,1,bloblen,fp);
			//fputc('\n',fp);
			fprintf(fp,"\r\n");
		}
		else{
			break;
		}
	}
	fclose(fp);
}

int main0(int argc,char **argv){
	if(argc==6){
		int keylen=strlen(argv[2]);
		char table[1024];
		char md5[256];
		GetMd5((unsigned char*)argv[4],strlen(argv[4]),(unsigned char*)md5);
		strcpy(table,*argv[3]=='f'?"mr_friend_":"mr_troop_");
		strcat(table,md5);
		strcat(table,"_New");
		
		sqlite3 *db=NULL;
		int rc=sqlite3_open(argv[1],&db);
		GetMsg(db,table,argv[2],keylen,argv[5]);
		sqlite3_close(db);
	}
	else{
		printf("msgexport v0.01 by flaribbit\n");
		printf("usage: msgexport $database_file $seckey group|friend $id $output_file\n");
	}
}

void GetNickName(){
	char buf[10240];
	//sprintf(buf,"SELECT memberuin,friendnick FROM TroopMemberInfo WHERE troopuin=%s",);
	sqlite3 *db=NULL;
	const char *key=KEY;
	int keylen=strlen((char *)key);
	int rc=sqlite3_open("2153059054.db",&db);//打开数据库
	sqlite3_stmt* stmt;
	sprintf(buf,"SELECT memberuin,friendnick FROM TroopMemberInfo LIMIT 10");
	sqlite3_prepare(db,buf,strlen(buf),&stmt,0);//查询结果
	#define GetString(col,buf,len) len=sqlite3_column_bytes(stmt,col);memcpy(buf,(const char*)sqlite3_column_text(stmt,col),len+1)
	for(;;){
		rc=sqlite3_step(stmt);
		if(rc==SQLITE_ROW){
			char s[1024];
			int len;
			GetString(0,s,len);
			TextDecode(s,len,key,keylen);
			printf(s);
			printf("\t");
			GetString(1,s,len);
			TextDecode(s,len,key,keylen);
			puts(s);
		}else{
			break;
		}
	}
}

int main(){
	GetNickName();
	return 0;
	char *key=KEY;
	int keylen=strlen((char *)key);
	sqlite3 *db=NULL;
	//打开数据库
	int rc=sqlite3_open("2153059054.db",&db);
	//获取数据
	char buf[10240];
	int bufsize;
	for(int i=1;i<=180;i++){
		GetBlob(db,0,"TroopMemberInfo","friendnick",i,buf,&bufsize);
		// TextDecode(buf,len,key,keylen);
		fwrite(buf,1,bufsize,stdout);
		putchar('\n');
	}
	sqlite3_close(db);
}
