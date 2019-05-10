#include <libcouchbase/couchbase.h>
#include <libcouchbase/n1ql.h>
#include <vector>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include "include/json/json.h"
#include <unistd.h>
#include <cstddef>
#include <functional> 
#include <algorithm>
#include <chrono>
#include <typeinfo>

using namespace std::chrono;

using namespace std;

struct Rows {
    std::vector< std::string > rows;
    std::string metadata;
    lcb_error_t rc;
    short htcode;
    Rows() : rc(LCB_ERROR), htcode(0) {}
};
struct Result {
    std::string value;
    lcb_error_t status;

    Result() : status(LCB_SUCCESS) {}
};

Result my_result;

lcb_create_st crst;
lcb_error_t rc;
int i = 0;
lcb_t instance;
int flag,flag1= 0;
char chk;

static void check(lcb_error_t err, const char *msg)
{
    if (err != LCB_SUCCESS) {
        fprintf(stderr, "[\x1b[31mERROR\x1b[0m] %s: %s\n", msg, lcb_strerror_short(err));
        exit(EXIT_FAILURE);
    }
}

static void http_callback(lcb_t, int, const lcb_RESPHTTP *resp)
{
    printf("Operation completed with HTTP code: %d\n", resp->htstatus);
    printf("Payload: %.*s\n", (int)resp->nbody, (char *)resp->body);
}

void store_callback(lcb_t instance, int cbtype, const lcb_RESPBASE *rb)
{
    if (rb->rc == LCB_SUCCESS) {
        printf("Store success: CAS=%llx\n", rb->cas);
    } else {
        printf("Store failed: %s\n", lcb_strerror(NULL, rb->rc));
    }
}
static void get_callback(lcb_t, int, const lcb_RESPBASE *rb)
{
    // "cast" to specific callback type
    const lcb_RESPGET *resp = reinterpret_cast< const lcb_RESPGET * >(rb);
    Result *my_result = reinterpret_cast< Result * >(rb->cookie);

    my_result->status = resp->rc;
    my_result->value.clear(); // Remove any prior value
    if (resp->rc == LCB_SUCCESS) {
        my_result->value.assign(reinterpret_cast< const char * >(resp->value), resp->nvalue);
    }
}

static void query_callback(lcb_t, int, const lcb_RESPN1QL *resp)
{

    Rows *rows = reinterpret_cast< Rows * >(resp->cookie);

    // Check if this is the last invocation

    if (resp->rflags & LCB_RESP_F_FINAL) {
        rows->rc = resp->rc;

        // Assign the metadata (usually not needed)
        rows->metadata.assign(resp->row, resp->nrow);

    }

    else {
        rows->rows.push_back(std::string(resp->row, resp->nrow));
    }
}


std::string get_str_between_two_str(const std::string &s,
        const std::string &start_delim,
        const std::string &stop_delim)
{
    unsigned first_delim_pos = s.find(start_delim);
    unsigned end_pos_of_first_delim = first_delim_pos + start_delim.length();
    unsigned last_delim_pos = s.find(stop_delim);

    return s.substr(end_pos_of_first_delim,
            last_delim_pos - end_pos_of_first_delim);
}

/// Returns the position of the 'Nth' occurrence of 'find' within 'str'.
/// Note that 1==find_Nth( "aaa", 2, "aa" ) [2nd "aa"]
/// - http://stackoverflow.com/questions/17023302/
size_t find_Nth(
    const std::string & str ,   // where to work
    unsigned            N ,     // N'th ocurrence
    const std::string & find    // what to 'find'
) {
    if ( 0==N ) { return std::string::npos; }
    size_t pos,from=0;
    unsigned i=0;
    while ( i<N ) {
        pos=str.find(find,from);
        if ( std::string::npos == pos ) { break; }
        from = pos + 1; // from = pos + find.size();
        ++i;
    }
    return pos;
/**
    It would be more efficient to use a variation of KMP to
    benefit from the failure function.
    - Algorithm inspired by James Kanze.
    - http://stackoverflow.com/questions/20406744/
*/
}

// returns count of non-overlapping occurrences of 'sub' in 'str'
int countSubstring(const std::string& str, const std::string& sub)
{
    if (sub.length() == 0) return 0;
    int count = 0;
    for (size_t offset = str.find(sub); offset != std::string::npos;
     offset = str.find(sub, offset + sub.length()))
    {
        ++count;
    }
    return count;
}

int CBUpdate(lcb_t instance ,const std::string& BucketId, const std::string& JsonDataUpdate)
{

    Json::Reader reader;
    Json::Value root;

    reader.parse(JsonDataUpdate.c_str(), root, false);

    Json::Value value = root["value"];
    Json::Value path = root["path"];

    //auto firstelem = entriesArray[0];
    //string sender = firstelem["sender"].asString();
    //int j = stoi(sender);
    cout << "PATH : " << path << "\n";
    cout << "VALUE : " << value << "\n";

    lcb_N1QLPARAMS *params;
    lcb_CMDN1QL cmd = {};
    Rows rows;

    lcb_error_t err;

    Json::FastWriter fastWriter;
    std::string Svalue = fastWriter.write(value);
    printf("\n%s\n",Svalue.c_str());
    Json::FastWriter fastWriter1;
    std::string Spath = fastWriter1.write(path);
    std::size_t posStart = Spath.find("nf-instances/");
    posStart = posStart+13;
    std::size_t posEnd = Spath.find("?");
    posEnd = posEnd - posStart;
    std::string nfInstanceID;
    nfInstanceID +='"';
    nfInstanceID += Spath.substr(posStart, posEnd);;
    nfInstanceID +='"';
    printf("\n%s\n",nfInstanceID.c_str());
    params = lcb_n1p_new();
    char Idx[1024] = {0};
    sprintf(Idx, "CREATE PRIMARY INDEX ON `%s` USING GSI;", BucketId.c_str());
    err = lcb_n1p_setstmtz(params, Idx);
    if (err != LCB_SUCCESS) {
        printf("failed: 0x%x (%s)\n",
             err, lcb_strerror(NULL, err));
        printf("\nerror : 2n");
        exit(EXIT_FAILURE);
    }
    cmd.callback = query_callback;
    err = lcb_n1p_mkcmd(params, &cmd);
    err = lcb_n1ql_query(instance, &rows, &cmd);
    lcb_wait(instance);
    sleep(5);
    char query[1024] = {0};
    snprintf(query, sizeof(query), "UPDATE `%s` doc SET doc = OBJECT_CONCAT(doc, $Value) WHERE nfInstanceID = $InstId;", BucketId.c_str());
    check(lcb_n1p_setstmtz(params, query),"set QUERY statement");
    check(lcb_n1p_namedparam(params,"$Value",strlen("$Value"), Svalue.c_str(), Svalue.length()), "set QUERY Named parameter") ;
    check(lcb_n1p_namedparam(params,"$InstId",strlen("$InstId"),nfInstanceID.c_str() ,nfInstanceID.length()),"set QUERY Named parameter" );

    cmd.callback = query_callback;
    rc = lcb_n1p_mkcmd(params, &cmd);
    rc = lcb_n1ql_query(instance, &rows, &cmd);
    lcb_wait(instance);

    if (rows.rc == LCB_SUCCESS) {
        std::cout << "Query successful!" << std::endl;
        std::vector< std::string >::iterator ii;
        for (ii = rows.rows.begin(); ii != rows.rows.end(); ++ii) {
            std::cout << *ii << std::endl;
        }
    } else {
        std::cerr << "Query failed!";
        std::cerr << "(" << int(rows.rc) << "). ";
        std::cerr << lcb_strerror(NULL, rows.rc) << std::endl;
    }

    lcb_n1p_free(params);
}
int CreateDoc(lcb_t instance, const std::string& JsonDataPath, const std::string& DocId,int TTL)
{
    printf("\n %s",JsonDataPath.c_str());

    std::string str = "/opt/VCM/config/";

    str.append(JsonDataPath.c_str());
    printf("\n%s\n",str.c_str());
    FILE* pFile = fopen(str.c_str(), "r");
    fseek (pFile , 0 , SEEK_END);
    int lSize = ftell(pFile);
    printf("lSize : %d\n",lSize);
    fseek (pFile , 0 , SEEK_SET);
    char jsonData[lSize];
    fread(&jsonData, sizeof(char), lSize, pFile);
    jsonData[lSize]='\0';
    fclose(pFile);
    std::string jsonStrData(jsonData);

    lcb_error_t oprc = LCB_SUCCESS;
    lcb_store_cmd_t stor;
    const lcb_store_cmd_t *store[1];

    store[0] = &stor;
    memset(&stor, 0, sizeof(stor));
    stor.v.v0.operation = LCB_SET;
    stor.v.v0.key       = DocId.c_str();
    stor.v.v0.nkey      = DocId.length();
    stor.v.v0.bytes     = jsonStrData.c_str();
    stor.v.v0.nbytes    = strlen(jsonStrData.c_str());
    if(TTL > 0){
    stor.v.v0.exptime   = TTL;
    }
    else{
    stor.v.v0.exptime   = 0;
    }
   /* stor.v.v0.cas       = cas;*/

    lcb_store(instance, NULL, 1, store);
    lcb_wait(instance);
    lcb_install_callback3(instance, LCB_CALLBACK_STORE, store_callback);
    return 0 ;
}
int DeleteDoc(lcb_t instance, const std::string& DocId)
{
    lcb_remove_cmd_t remove ;
    const lcb_remove_cmd_t* commands[1] ;
    commands[0] = &remove;
    memset(&remove, 0, sizeof(remove));
    remove.version    = 0;
    remove.v.v0.key   = DocId.c_str();
    remove.v.v0.nkey  = DocId.length() ;
    //remove->v.v0.cas   = 0x666;
    lcb_remove(instance, NULL, 1, commands);
    lcb_wait(instance);
}

int delete_bucket(lcb_t instance)
{

    lcb_CMDHTTP htcmd = {};
    memset(&htcmd, 0, sizeof htcmd);
    std::string path = "/pools/default/buckets/default";
    LCB_CMD_SET_KEY(&htcmd, path.c_str(), path.size());
    htcmd.method = LCB_HTTP_METHOD_DELETE;
    htcmd.type = LCB_HTTP_TYPE_MANAGEMENT;
    htcmd.username = "Administrator";
    htcmd.password = "abc123";
    lcb_http3(instance, NULL, &htcmd);
    lcb_wait(instance);
    printf("\nDelete\n");
}

lcb_t Connect(const std::string& BucketName)
{
    crst.version = 3;
    string conn = "couchbase://10.1.31.171/";
    conn += BucketName.c_str();
    crst.v.v3.connstr = conn.c_str();
    //crst.v.v3.username = "Administrator";
    //crst.v.v3.passwd = "abc123";
    char *bucket = NULL;
    check(lcb_create(&instance, &crst), "create couchbase handle");
    check(lcb_connect(instance), "schedule connection");
    lcb_wait(instance);
    check(lcb_get_bootstrap_status(instance), "bootstrap from cluster" );
    check(lcb_cntl(instance, LCB_CNTL_GET, LCB_CNTL_BUCKETNAME, &bucket), "get bucket name");
    return instance;
}

lcb_t create_bucket(lcb_t instance, const std::string& BucketName)
{
    char *bucket = NULL;
    check(lcb_cntl(instance, LCB_CNTL_GET, LCB_CNTL_BUCKETNAME, &bucket), "get bucket name");
    // Create the required parameters according to the Couchbase REST API
    std::string path("/pools/default/buckets");
    if(strcmp(bucket,BucketName.c_str()))
    {
    std::string params;
    params += "name=";
    params += BucketName.c_str();
    params += "&";
    params += "bucketType=couchbase&";

    // authType should always be SASL. You can leave the saslPassword field
    // empty if you don't want to protect this bucket.
    params += "authType=sasl&saslPassword=&";
    params += "ramQuotaMB=100";

    lcb_CMDHTTP htcmd = {};
    LCB_CMD_SET_KEY(&htcmd, path.c_str(), path.size());
    htcmd.body = params.c_str();
    htcmd.nbody = params.size();
    htcmd.content_type = "application/x-www-form-urlencoded";
    htcmd.method = LCB_HTTP_METHOD_POST;
    htcmd.type = LCB_HTTP_TYPE_MANAGEMENT;
    htcmd.username = "Administrator";
    htcmd.password = "abc123";
    check(lcb_http3(instance, NULL, &htcmd),"HTTP new Bucket");
    lcb_wait(instance);
    //lcb_install_callback3(instance, LCB_CALLBACK_HTTP, (lcb_RESPCALLBACK)http_callback);
    return Connect(BucketName.c_str());
    }
    else
    {
        return Connect(BucketName.c_str());
    }
}

inline unsigned char from_hex (
        unsigned char ch
    ) 
{
    if (ch <= '9' && ch >= '0')
        ch -= '0';
    else if (ch <= 'f' && ch >= 'a')
        ch -= 'a' - 10;
    else if (ch <= 'F' && ch >= 'A')
        ch -= 'A' - 10;
    else 
        ch = 0;
    return ch;
}

const std::string uridecode (
        const std::string& str
    ) 
{
    using namespace std;
    string result;
    string::size_type i;
    for (i = 0; i < str.size(); ++i)
    {
        if (str[i] == '+')
        {
            result += ' ';
        }
        else if (str[i] == '%' && str.size() > i+2)
        {
            const unsigned char ch1 = from_hex(str[i+1]);
            const unsigned char ch2 = from_hex(str[i+2]);
            const unsigned char ch = (ch1 << 4) | ch2;
            result += ch;
            i += 2;
        }
        else
        {
            result += str[i];
        }
}
return result;
}
const char * getquerystrfromuri(const std::string& uri, const std::string& BucketName)
{
    std::size_t posStart ;
    char substr[3024];
//    char Query[4048] = "SELECT Distinct(mybucket.nfInstanceID),mybucket.nfType,mybucket.nfStatus,mybucket.plmn,mybucket.sNssais,mybucket.nsiList,mybucket.fqdn,mybucket.interPlmnFqdn,mybucket.ipv4Addresses,mybucket.ipv6Addresses,mybucket.priority,mybucket.capacity,mybucket.load,mybucket.locality,mybucket.udrInfo,mybucket.udmInfo,mybucket.ausfInfo,mybucket.amfInfo,mybucket.smfInfo,mybucket.upfInfo,mybucket.pcfInfo,mybucket.bsfInfo,mybucket.nfServices";
    char Query[4048] = "SELECT Distinct(mybucket.nfInstanceID) ";
    sprintf(Query + strlen(Query)," FROM `%s`as mybucket UNNEST mybucket.nfServices as services WHERE ",  BucketName.c_str());
    string uritostr = uridecode(uri);
    printf("\nUriStr : %s\n",uritostr.c_str());
    std::string targetnfType ;
    /* target-nf-type, requester-nf-type are
     * mandatory params.
     */
    if(strstr(uritostr.c_str(),"target-nf-type") != NULL  && strstr(uritostr.c_str(),"requester-nf-type") != NULL)
    {
        if(strstr(uritostr.c_str(),"target-nf-instance-id") != NULL)
        {

            posStart = uritostr.find("target-nf-instance-id");
            posStart = posStart+21;
            std::string nfInstance = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
            sprintf(Query + strlen(Query), "mybucket.nfInstanceID = %s",nfInstance.c_str());
            printf("\nFinal Query string : %s\n",Query);
        }
        else
        {
            posStart = uritostr.find("target-nf-type");
            posStart = posStart;
            targetnfType = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
            sprintf(Query + strlen(Query), "mybucket.nfType = %s", targetnfType.c_str());

            if(strcmp(targetnfType.c_str(),"\"UPF\"") != 0)
            {
/*                posStart = uritostr.find("requester-nf-type");
                posStart = posStart;
                std::string requesternfType = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                sprintf(Query + strlen(Query), " and ANY reqnf IN services.allowedNfTypes SATISFIES reqnf IN [%s] end", requesternfType.c_str());
*/  
          }

            /* unnest shrinidhi need to confirm : sushmit da,regarding array pattern ([] or repeated  in query/uri */
            if(strstr(uritostr.c_str(),"service-names") != NULL)
            {
                for(int occurrence = 1; occurrence <= countSubstring(uritostr.c_str(), "service-names"); occurrence++)
                {
                    posStart = find_Nth(uritostr.c_str(), occurrence, "service-names");
                    posStart = posStart;
                    std::string servicenames = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    if(occurrence == 1)
                    {
                        sprintf(Query + strlen(Query), " and services.serviceName = %s  ", servicenames.c_str());
                    }
                    else
                    {
                        sprintf(Query + strlen(Query), "or services.serviceName = %s ", servicenames.c_str());
                    }
                }
            }

            if(strstr(uritostr.c_str(),"requester-nf-instance-fqdn") != NULL)
            {
                posStart = uritostr.find("requester-nf-instance-fqdn");
                posStart = posStart;
                std::string reqnfinstancefqdn = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                sprintf(Query + strlen(Query), "and ANY allowedfqdn IN services.allowedNfDomains SATISFIES allowedfqdn IN [%s] end", reqnfinstancefqdn.c_str());
            }

            if(strstr(uritostr.c_str(),"target-plmn") != NULL)
            {
                posStart = uritostr.find("target-plmn");
                posStart = posStart;
                std::string targetplmn = get_str_between_two_str( uritostr.substr(posStart), "=", "&") ;
                sprintf(Query + strlen(Query), "and mybucket.plmn = %s", targetplmn.c_str());
            }

            if(strstr(uritostr.c_str(),"requester-plmn") != NULL )
            {
                posStart = uritostr.find("requester-plmn");
                posStart = posStart;
                std::string requesterplmn = get_str_between_two_str( uritostr.substr(posStart), "=", "&") ;
                sprintf(Query + strlen(Query), "and ANY reqplmn IN services.allowedPlmns SATISFIES reqplmn IN [%s] end", requesterplmn.c_str());
            }

            if(strstr(uritostr.c_str(),"target-nf-fqdn") != NULL)
            {
                posStart = uritostr.find("target-nf-fqdn=");
                posStart = posStart;
                std::string targetnffqdn = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                sprintf(Query + strlen(Query), "and mybucket.fqdn = %s", targetnffqdn.c_str());
            }
            /*Need to discusses with sushmit da */
            if(strstr(uritostr.c_str(),"hnrf-uri") != NULL)
            {
                posStart = uritostr.find("hnrf-uri");
                posStart = posStart;
                std::string targetnffqdn = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                sprintf(Query + strlen(Query), "and mybucket.fqdn = %s", targetnffqdn.c_str());
            }

            if(strstr(uritostr.c_str(),"snssais") != NULL)
            {
                for(int occurrence = 1; occurrence <= countSubstring(uritostr.c_str(), "snssais"); occurrence++)
                {
                    posStart = find_Nth(uritostr.c_str(), occurrence, "snssais");
                    posStart = posStart;
                    std::string Snssais = get_str_between_two_str( uritostr.substr(posStart), "=", "&") ;
                    sprintf(Query + strlen(Query), " and any snssai IN mybucket.sNssais SATISFIES snssai IN [%s] end ", Snssais.c_str());
                }
            }

            if(strstr(uritostr.c_str(),"nsi-list") != NULL)
            {
                for(int occurrence = 1; occurrence <= countSubstring(uritostr.c_str(), "nsi-list"); occurrence++)
                {
                    posStart = find_Nth(uritostr.c_str(), occurrence, "nsi-list");
                    posStart = posStart;
                    std::string Nsilist =  get_str_between_two_str( uritostr.substr(posStart), "=", "&") ;
                    sprintf(Query + strlen(Query), " and any nsilist IN mybucket.nsiList SATISFIES nsilist IN [%s] end ", Nsilist.c_str());
                }
            }

            if(strcmp(targetnfType.c_str(),"\"AMF\"") == 0)
            {
                if(strstr(uritostr.c_str(),"tai") != NULL)
                {
                    posStart = uritostr.find("tai");
                    posStart = posStart;
                    std::string taistr = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), "any tai IN amfinfo.taiList SATISFIES tai IN [%s] end ", taistr.c_str());
                }

                if(strstr(uritostr.c_str(),"amf-region-id") != NULL)
                {
                    posStart = uritostr.find("amf-region-id");
                    posStart = posStart;
                    std::string amfregionid = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), "mybucket.amfInfo.amfRegionId =%s ", amfregionid.c_str());
                }
                if(strstr(uritostr.c_str(),"amf-set-id") != NULL)
                {
                    posStart = uritostr.find("amf-set-id");
                    posStart = posStart;
                    std::string amfsetid = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), "mybucket.amfInfo.amfSetId =%s ", amfsetid.c_str());
                }

                if(strstr(uritostr.c_str(),"guami") != NULL)
                {
                    posStart = uritostr.find("guami");
                    posStart = posStart;
                    std::string guamistr = get_str_between_two_str( uritostr.substr(posStart), "=", "&") ;
                    sprintf(Query + strlen(Query), "any guami IN amfinfo.guamiList SATISFIES guami IN [%s] end ", guamistr.c_str());
                }
                if(strstr(uritostr.c_str(),"supi") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.udmInfo.supiRanges as range ");
                    strcat(Query,substr);
                    posStart = uritostr.find("supi");
                    posStart = posStart;
                    std::string amfsetid = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and range.`start`<=%1$s and range.`end`>=%1$s ", amfsetid.c_str());
                }
            }
            if(strcmp(targetnfType.c_str(),"\"SMF\"") == 0)
            {
                if(strstr(uritostr.c_str(),"dnn") != NULL)
                {
                    posStart = uritostr.find("dnn");
                    posStart = posStart;
                    std::string dnnstr = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and any Dnn IN mybucket.smfInfo.dnnList SATISFIES Dnn IN [%s] end ", dnnstr.c_str());
                }

                if(strstr(uritostr.c_str(),"tai") != NULL)
                {
                    posStart = uritostr.find("tai");
                    posStart = posStart;
                    std::string taistr = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and any tai IN amfinfo.taiList SATISFIES tai IN [%s] end ", taistr.c_str());
                }

                if(strstr(uritostr.c_str(),"pgw-ind") != NULL)
                {
                    posStart = uritostr.find("pgw-ind");
                    posStart = posStart;
                    std::string pgwind = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    if(strcmp(pgwind.c_str(),"true") == 0)
                    {
                        posStart = string(Query).find("FROM");
                        posStart = posStart + 4;
                        strcpy(substr,Query + posStart);
                        strcpy(Query ," SELECT Distinct(mybucket.nfInstanceID),mybucket.smfInfo.pgwFqdn ");
                        strcat(Query,substr);

                    }
                }
                if(strstr(uritostr.c_str(),"pgw") != NULL)
                {
                    posStart = uritostr.find("pgw");
                    posStart = posStart;
                    std::string pgwfqdn = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and mybucket.smfInfo.pgwFqdn = %s ", pgwfqdn.c_str());
                }
            }
            if(strcmp(targetnfType.c_str(),"\"AUSF\"") == 0)
            {
                if(strstr(uritostr.c_str(),"supi") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.ausfInfo.supiRanges as range ");
                    strcat(Query,substr);
                    posStart = uritostr.find("supi");
                    posStart = posStart;
                    std::string Supi = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and range.`start`<=%1$s and range.`end`>=%1$s ", Supi.c_str());
                }
                if(strstr(uritostr.c_str(),"routing-indicator") != NULL)
                {
                    posStart = uritostr.find("routing-indicator");
                    posStart = posStart;
                    std::string Routingindicator = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and any routingindicator in mybucket.ausfInfo.routingIndicators SATISFIES routingindicator IN [%s] ", Routingindicator.c_str());
                }
                if(strstr(uritostr.c_str(),"group-id-list") != NULL)
                {
                    for(int occurrence = 1; occurrence <= countSubstring(uritostr.c_str(), "group-id-list"); occurrence++)
                    {
                        posStart = find_Nth(uritostr.c_str(), occurrence, "group-id-list");
                        posStart = posStart;
                        std::string groupid = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                        if(occurrence == 1)
                        {
                            sprintf(Query + strlen(Query), " and mybucket.ausfInfo.groupId = %s ", groupid.c_str());
                        }
                        else
                        {
                            sprintf(Query + strlen(Query), " or mybucket.ausfInfo.groupId = %s ", groupid.c_str());
                        }
                    }
                }
            }
            if(strcmp(targetnfType.c_str(),"\"NSSF\"") == 0)
            {
                /* ----- */
            }
            if(strcmp(targetnfType.c_str(),"\"UDM\"") == 0)
            {
                if(strstr(uritostr.c_str(),"supi") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.udmInfo.supiRanges as range ");
                    strcat(Query,substr);
                    posStart = uritostr.find("supi");
                    posStart = posStart;
                    std::string Supi = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and range.`start`<=%1$s and range.`end`>=%1$s ", Supi.c_str());
                }
                if(strstr(uritostr.c_str(),"gpsi") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.udmInfo.gpsiRanges as range ");
                    strcat(Query,substr);
                    posStart = uritostr.find("gpsi");
                    posStart = posStart;
                    std::string Gpsi = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and range.`start`<=%1$s and range.`end`>=%1$s ", Gpsi.c_str());
                }
                if(strstr(uritostr.c_str(),"external-group-identity") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.udmInfo.externalGroupIdentifiersRanges as range ");
                    strcat(Query,substr);
                    posStart = uritostr.find("external-group-identity");
                    posStart = posStart;
                    std::string ExternalGroupIdentifiersRanges = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and range.`start`<=%1$s and range.`end`>=%1$s ", ExternalGroupIdentifiersRanges.c_str());
                }
                if(strstr(uritostr.c_str(),"routing-indicator") != NULL)
                {
                    posStart = uritostr.find("routing-indicator");
                    posStart = posStart;
                    std::string Routingindicator = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and any routingindicator in mybucket.udmInfo.routingIndicators SATISFIES routingindicator IN [%s] ", Routingindicator.c_str());
                }
                if(strstr(uritostr.c_str(),"group-id-list") != NULL)
                {
                    for(int occurrence = 1; occurrence <= countSubstring(uritostr.c_str(), "group-id-list"); occurrence++)
                    {
                        posStart = find_Nth(uritostr.c_str(), occurrence, "group-id-list");
                        posStart = posStart;
                        std::string groupid = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                        if(occurrence == 1)
                        {
                            sprintf(Query + strlen(Query), " and mybucket.udmInfo.groupId = %s ", groupid.c_str());
                        }
                        else
                        {
                            sprintf(Query + strlen(Query), " or mybucket.udmInfo.groupId = %s ", groupid.c_str());
                        }
                    }
                }
            }
            if(strcmp(targetnfType.c_str(),"\"UDR\"") == 0)
            {
                if(strstr(uritostr.c_str(),"supi") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.udrInfo.supiRanges as range ");
                    strcat(Query,substr);
                    posStart = uritostr.find("supi");
                    posStart = posStart;
                    std::string Supi = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and range.`start`<=%1$s and range.`end`>=%1$s ", Supi.c_str());
                }
                if(strstr(uritostr.c_str(),"gpsi") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.udrInfo.gpsiRanges as range ");
                    strcat(Query,substr);
                    posStart = uritostr.find("gpsi");
                    posStart = posStart;
                    std::string Gpsi = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and range.`start`<=%1$s and range.`end`>=%1$s ", Gpsi.c_str());
                }
                if(strstr(uritostr.c_str(),"external-group-identity") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.udrInfo.externalGroupIdentifiersRanges as range ");
                    strcat(Query,substr);
                    posStart = uritostr.find("external-group-identity");
                    posStart = posStart;
                    std::string ExternalGroupIdentifiersRanges = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and range.`start`<=%1$s and range.`end`>=%1$s ", ExternalGroupIdentifiersRanges.c_str());
                }
                if(strstr(uritostr.c_str(),"data-set") != NULL)
                {
                    posStart = uritostr.find("data-set");
                    posStart = posStart;
                    std::string Dataset = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and any dataset in mybucket.udrInfo.supportedDataSets SATISFIES dataset IN [%s] ", Dataset.c_str());
                }
                if(strstr(uritostr.c_str(),"group-id-list") != NULL)
                {
                    for(int occurrence = 1; occurrence <= countSubstring(uritostr.c_str(), "group-id-list"); occurrence++)
                    {
                        posStart = find_Nth(uritostr.c_str(), occurrence, "group-id-list");
                        posStart = posStart;
                        std::string groupid = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                        if(occurrence == 1)
                        {
                            sprintf(Query + strlen(Query), " and mybucket.udrInfo.groupId = %s ", groupid.c_str());
                        }
                        else
                        {
                            sprintf(Query + strlen(Query), " or mybucket.udrInfo.groupId = %s ", groupid.c_str());
                        }

                    }
                }
            }
            if(strcmp(targetnfType.c_str(),"\"PCF\"") == 0)
            {
                if(strstr(uritostr.c_str(),"supi") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.pcfInfo.supiRanges as range ");
                    strcat(Query,substr);
                    posStart = uritostr.find("supi");
                    posStart = posStart;
                    std::string Supi = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and range.`start`<=%1$s and range.`end`>=%1$s ", Supi.c_str());
                }
            }
            if(strcmp(targetnfType.c_str(),"\"BSF\"") == 0)
            {
                posStart = string(Query).find("as mybucket");
                posStart = posStart + 12;
                strcpy(substr,Query + posStart);
                strcpy(Query + posStart," unnest mybucket.upfInfo as upfinfo ");
                strcat(Query,substr);
                if(strstr(uritostr.c_str(),"ue-ipv4-address") != NULL)
                {
                    /*TDB*/
                }
                if(strstr(uritostr.c_str(),"ue-ipv6-prefix") != NULL)
                {
                    /*TDB*/
                }
            }
            if(strcmp(targetnfType.c_str(),"\"UPF\"") == 0)
            {
                posStart = string(Query).find(" UNNEST mybucket.nfServices as services");
                posStart = posStart + 39;
                strcpy(substr,Query + posStart);
                strcpy(Query + posStart - 39, substr);

                if(strstr(uritostr.c_str(),"dnn") != NULL)
                {
                    posStart = string(Query).find("as mybucket");
                    posStart = posStart + 12;
                    strcpy(substr,Query + posStart);
                    strcpy(Query + posStart," unnest mybucket.upfInfo.sNssaiUpfInfoList as dnn ");
                    strcat(Query,substr);
                    posStart = uritostr.find("dnn");
                    posStart = posStart;
                    std::string dnnstr = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and any Dnn IN dnn.dnnUpfInfoList SATISFIES Dnn IN [%s] end ", dnnstr.c_str());
                }
                if(strstr(uritostr.c_str(), "smf-serving-area") != NULL)
                {
                    posStart = uritostr.find("smf-serving-area");
                    posStart = posStart;
                    std::string smfservingarea = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                    sprintf(Query + strlen(Query), " and any smfservingarea IN mybucket.upfInfo.smfServingArea SATISFIES smfservingarea IN [%s] end ", smfservingarea.c_str());

                }
            }

            if(strstr(uritostr.c_str(),"supported-features") != NULL)
            {
                posStart = uritostr.find("supported-features");
                posStart = posStart;
                std::string supportedfeatures = '"'+ get_str_between_two_str( uritostr.substr(posStart), "=", "&") + '"';
                sprintf(Query + strlen(Query), " and services.supportedFeatures = %s ", supportedfeatures.c_str());
            }

           printf("\nFinal Query string : %s\n",Query);
        }
    }
    else
    {
        printf("\nMandatory params are missing\n");
    }
    return Query;
}
void DiscoveryQuery(lcb_t instance,const std::string& BucketName, const std::string& QueryStr)
{

    lcb_N1QLPARAMS *params;
    lcb_CMDN1QL cmd = {};
    Rows rows;

    params = lcb_n1p_new();
    check(lcb_n1p_setstmtz(params, QueryStr.c_str()),"Set Query Command");
    check(lcb_n1p_mkcmd(params, &cmd),"Query Command");
    cmd.callback = query_callback;
    check(lcb_n1ql_query(instance, &rows, &cmd),"Querying ....");
    lcb_wait(instance);

    if (rows.rc == LCB_SUCCESS) {
        std::cout << "Query successful!" << std::endl;
        std::vector< std::string >::iterator ii;
        for (ii = rows.rows.begin(); ii != rows.rows.end(); ++ii) {
            std::cout << *ii << std::endl;
        }

    } else {
        std::cerr << "Query failed!";
        std::cerr << "(" << int(rows.rc) << "). ";
        std::cerr << lcb_strerror(NULL, rows.rc) << std::endl;
    }

    lcb_n1p_free(params);

}

int Query(lcb_t instance,const std::string& BucketName, const std::string& UriQuery)
{
    lcb_N1QLPARAMS *params;
    lcb_CMDN1QL cmd = {};
    Rows rows;
    char Query[1024];

    params = lcb_n1p_new();
    check(lcb_n1p_setstmtz(params, "CREATE PRIMARY INDEX ON `Bhushan_bucket` USING GSI;"),"Indexing....");
    cmd.callback = query_callback;
    /*Populates the given low-level lcb_CMDN1QL structure with the relevant fields from the params structure.*/
    check(lcb_n1p_mkcmd(params, &cmd),"Populates the given low-level lcb_CMDN1QL structure with the relevant fields from the params structure");
    check(lcb_n1ql_query(instance, &rows, &cmd),"Execute a N1QL query.");
    lcb_wait(instance);

    sleep(2);

    std::size_t posStart = UriQuery.find("target-nf-type=");
    posStart = posStart+15;
    std::string TnfType = UriQuery.substr(posStart);
    std::size_t posEnd = TnfType.find("&");

    posStart = UriQuery.find("target-plmn=");
    posStart = posStart+12;
    std::string Tplmn = UriQuery.substr(posStart);
    std::string mcc = '"'+ get_str_between_two_str(Tplmn, "mcc%22%3A%22", "%22%2C%22") + '"';
    std::string mnc ='"' + get_str_between_two_str(Tplmn, "mnc%22%3A%22", "%22%7D") + '"';

    sprintf(Query, "SELECT * FROM `%s`" " WHERE ",BucketName.c_str());
    //sprintf(Query, "SELECT * FROM `%s`" " WHERE ","vcmControlSessionDb");
/*    if(ServiceNm.substr(0,posEnd).c_str() != NULL)
    {
        strcat(Query, " serviceName:");
        strcat(Query,( char*)ServiceNm.substr(0,posEnd).c_str());
    }*/
    if(TnfType.substr(0,posEnd).c_str() != NULL)
    {
        strcat(Query, "nfType = ");
        string ts;
        ts += '"' ;
        ts += TnfType.substr(0,posEnd).c_str();
        ts += '"';
        printf("\n%s\n",ts.c_str());
        strcat(Query, ts.c_str());
        char TplmnQ[100];
//        sprintf(TplmnQ, "and ANY h IN OBJECT_VALUES(%s) SATISFIES h.mcc = %s and h.mnc= %s END ;",BucketName.c_str(),mcc.c_str()            ,mnc.c_str());
  //      strncat(Query, TplmnQ,strlen(TplmnQ));
        Query[strlen(Query)]= NULL;
    }
    printf("\n%s\n",Query);

    check(lcb_n1p_setstmtz(params, Query),"Set Query Command");
    check(lcb_n1p_mkcmd(params, &cmd),"Query Command");
    cmd.callback = query_callback;
    check(lcb_n1ql_query(instance, &rows, &cmd),"Querying ....");
    lcb_wait(instance);

    if (rows.rc == LCB_SUCCESS) {
        std::cout << "Query successful!" << std::endl;

        std::vector< std::string >::iterator ii;
        for (ii = rows.rows.begin(); ii != rows.rows.end(); ++ii) {
            std::cout << *ii << std::endl;
        }

    } else {
        std::cerr << "Query failed!";
        std::cerr << "(" << int(rows.rc) << "). ";
        std::cerr << lcb_strerror(NULL, rows.rc) << std::endl;
    }

    lcb_n1p_free(params);

}
Result jsonretrieve(lcb_t instance,const std::string& RetDocId)
{
    lcb_install_callback3(instance, LCB_CALLBACK_GET, get_callback);
    lcb_CMDGET cmd = { 0 };
    LCB_CMD_SET_KEY(&cmd, RetDocId.c_str(), RetDocId.length());
    lcb_get3(instance, &my_result, &cmd);
    lcb_wait(instance);
    return my_result;

}

void * JsonParse(lcb_t instance,const std::string& RetDocId)
{

    Result my_result = jsonretrieve(instance,RetDocId.c_str());

    char * pEnd;

    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse( my_result.value, root );
    if ( !parsingSuccessful )
    {
        cout << "Error parsing the string" << endl;
    }

    const Json::Value UdmInfoList = root["UdmInfoList"];
    
    unsigned long int supi = 123456789060001;
    unsigned long start;
    unsigned long end;
    vector<string> keys;
    for ( int index = 0; index < UdmInfoList.size(); ++index) 
    {
        for(int index1 = 0 ; index1 < UdmInfoList[index]["supirange"].size(); index1++)
        {
            start = strtoul (string(UdmInfoList[index]["supirange"][index1]["start"].asString()).c_str(),&pEnd,10);
            end = strtoul ((UdmInfoList[index]["supirange"][index1]["end"].asString()).c_str(),&pEnd,10);
            if(supi >= start && supi <= end) 
            {
                printf("\n start : %ld end : %ld supi : %ld nfInstance:%s \n", start, end ,supi ,string(UdmInfoList[index]["nfInstanceid"].asString()).c_str());
                keys.push_back(string(UdmInfoList[index]["nfInstanceid"].asString()).c_str());
                break;
            }
         }
     }
     for (auto i = keys.begin(); i != keys.end(); ++i)
     {
         Result my_result = jsonretrieve(instance,*i);
         std::cout << "Status for getting " <<  *i << ": ";
         std::cout << lcb_strerror(NULL, my_result.status);
         std::cout << "Value: " << my_result.value << std::endl;
     }
}

int main(int, char **)
{
    int i;
    char keyId[1024];
    printf("Connecting .... CouchB\n");
    lcb_t instance = Connect("vcmBsfBindingDataDb");
    printf("\nCreating NewBucket ON CouchB\n");
    instance = create_bucket(instance,"vcmBsfBindingDataDb");

    sleep(4);
    do{
          std :: string JsonDataPath, DocId;
          std::cout << "Enter JsonFileName to Store in CB:";
          std::getline(std::cin, JsonDataPath);

          std::cout << "Enter DocId:";
          std::getline(std::cin,DocId);
          int TTL = 60;
          printf("\n JsonDataPath size : %d \n",JsonDataPath.length());
auto start = high_resolution_clock::now();

          CreateDoc(instance, JsonDataPath, DocId, TTL);

auto stop = high_resolution_clock::now();
auto duration = duration_cast<microseconds>(stop - start);

cout << "Time taken by createdoc " << duration.count() << " microseconds" << endl;
          printf("Do you want to Store More doc in Cb [yes : 1/ No : 0]\n");
          scanf("%d",&flag);
          std :: cin.ignore();

       }while(flag != 0);

/*
auto start = high_resolution_clock::now();
       JsonParse(instance,"test_2");
auto stop = high_resolution_clock::now();
auto duration = duration_cast<microseconds>(stop - start);

cout << "Time taken by JsonParse " << duration.count() << " microseconds" << endl;
*/
/*       do{
             Result my_result;
             std :: string RetDocId;
             std::cout << "Enter DocId to Retrieve:";
             std::getline(std::cin,RetDocId);
             lcb_CMDGET cmd = { 0 };
             LCB_CMD_SET_KEY(&cmd, RetDocId.c_str(), RetDocId.length());
             lcb_get3(instance, &my_result, &cmd);
             lcb_wait(instance);
             lcb_install_callback3(instance, LCB_CALLBACK_GET, get_callback);
             sleep(4);
             std::cout << "Status for getting " <<  RetDocId.c_str() << ": ";
             std::cout << lcb_strerror(NULL, my_result.status);
             std::cout << ". Value: " << my_result.value << std::endl;

             printf("Do you want to Retrieve More doc in Cb [yes : 1/ No : 0]\n");
             scanf("%d",&flag);
             std :: cin.ignore();
         }while(flag != 0);
    sleep(1);
    do{
          std :: string UriQuery;
          std::cout << "Enter Discovery uri for Query :";
          std::getline(std::cin, UriQuery);
          DiscoveryQuery(instance, "Bhushan_bucket", getQueryString(UriQuery));
          printf("Do you want to Continue with Query [yes : 1/ No : 0]\n");
          scanf("%d",&flag1);
          UriQuery.erase();
          std :: cin.ignore();

      }while(flag1 != 0);
    sleep(1);
     do{
          std :: string JsonDataUpdate;
          std::cout << "Enter String with PATH AND VALUE for Doc Update:";
          std::getline(std::cin, JsonDataUpdate);

          CBUpdate(instance, "Bhushan_bucket", JsonDataUpdate);
          printf("Do you want to Update Doc in Couchbase [yes : 1/ No : 0]\n");
          scanf("%d",&flag1);
          std :: cin.ignore();

       }while(flag1 != 0);
   */
//    DeleteDoc(instance,"nssf");
//    delete_bucket(instance);
//    lcb_destroy(instance);*/

      /* Nssai reqnftype reqplmn servicename */
//        string uri = "%2Fnnrf-disc%2Fv1%2Fnf-instances%3Frequester-nf-type%3DUPF%26service-names%3DAMF1.Service1%26target-nf-type%3DAMF%26target-plmn%3D%7B%22mcc%22%3A%22405%22%2C%22mnc%22%3A%2205%22%7D%26requester-plmn%3D%7B%22mcc%22%3A%22405%22%2C%22mnc%22%3A%2205%22%7D%26snssais%3D%7B%22sst%22%3A1%2C%22sd%22%3A%221%22%7D%26snssais%3D%7B%22sst%22%3A2%2C%22sd%22%3A%222%22%7D%26snssais%3D%7B%22sst%22%3A3%2C%22sd%22%3A%223%22%7D";

      /* Nssai reqnftype reqplmn servicename[] */
     //   string uri = "%2Fnnrf-disc%2Fv1%2Fnf-instances%3Frequester-nf-type%3DUPF%26service-names%3DAMF1.Service1%26service-names%3DAMF1.Service2%26target-nf-type%3DAMF%26target-plmn%3D%7B%22mcc%22%3A%22405%22%2C%22mnc%22%3A%2205%22%7D%26requester-plmn%3D%7B%22mcc%22%3A%22405%22%2C%22mnc%22%3A%2205%22%7D%26snssais%3D%7B%22sst%22%3A1%2C%22sd%22%3A%221%22%7D%26snssais%3D%7B%22sst%22%3A2%2C%22sd%22%3A%222%22%7D%26snssais%3D%7B%22sst%22%3A3%2C%22sd%22%3A%223%22%7D";

      /*Group Id list*/
//    string uri = "nnrf-disc%2Fv1%2Fnf-instances%3Frequester-nf-type%3DSMF%26target-nf-type%3DUDM%26group-id-list%3Dgroup3%26group-id-list%3Dgroup1";

      /* UPF smf-serving-area */
//      string uri = "nnrf-disc%2Fv1%2Fnf-instances%3Frequester-nf-type%3DSMF%26target-nf-type%3DUPF%26smf-serving-area%3DservingArea3";

      /* Supi */
      string uri = "%2Fnnrf-disc%2Fv1%2Fnf-instances%3Frequester-nf-type%3DAUSF%26service-names%3DNudm_UEAuthentication%26target-nf-type%3DUDM%26target-plmn%3D%7B%22mcc%22%3A%22405%22%2C%22mnc%22%3A%2205%22%7D%26requester-plmn%3D%7B%22mcc%22%3A%22405%22%2C%22mnc%22%3A%2205%22%7D%26supi%3D123456789079999";

//        string uri = "/nnrf-disc/v1/nf-instances?requester-nf-type=UPF&target-nf-type=AMF";
        //string s1 = getquerystrfromuri(uri,"NRF");
        //string s1 = "SELECT Distinct(mybucket.nfInstanceID)  FROM `NRF` as mybucket  unnest mybucket.udmInfo.supiRanges as range UNNEST mybucket.nfServices as services WHERE mybucket.nfType = \"UDM\" and range.`start`<=\"823456789040000\" and range.`end`>=\"823456789040000\" ";
        string s1 = "SELECT dnn FROM `vcmBsfBindingDataDb` as a WHERE a.ipv4Address = \"10.32.30.10\"; ";

auto start = high_resolution_clock::now();

        DiscoveryQuery(instance, "vcmBsfBindingDataDb",  s1);

auto stop = high_resolution_clock::now();
auto duration = duration_cast<microseconds>(stop - start);
cout << "Time taken by n1qlQuery " << duration.count() << " microseconds" << endl;
/*
auto start = high_resolution_clock::now();

         Result my_result = jsonretrieve(instance,"Document3");
         std::cout << "Status for getting " << "Document3"  << ": ";
         std::cout << lcb_strerror(NULL, my_result.status);
         std::cout << "ValueSize: " << my_result.value.length() << std::endl;

auto stop = high_resolution_clock::now();
auto duration = duration_cast<microseconds>(stop - start);
cout << "Time taken by jsonretrieve " << duration.count() << " microseconds" << endl;
*/
}
