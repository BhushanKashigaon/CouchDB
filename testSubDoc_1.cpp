#define LCB_NO_DEPR_CXX_CTORS
#undef NDEBUG

#include <libcouchbase/couchbase.h>
#include <libcouchbase/api3.h>
#include <assert.h>
#include <assert.h>
#include <string.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>

static void
op_callback(lcb_t, int cbtype, const lcb_RESPBASE *rb)
{
    fprintf(stderr, "Got callback for %s.. ", lcb_strcbtype(cbtype));
    if (rb->rc != LCB_SUCCESS && rb->rc != LCB_SUBDOC_MULTI_FAILURE) {
        fprintf(stderr, "Operation failed (%s)\n", lcb_strerror(NULL, rb->rc));
        return;
    }

    if (cbtype == LCB_CALLBACK_GET) {
        const lcb_RESPGET *rg = reinterpret_cast<const lcb_RESPGET*>(rb);
        fprintf(stderr, "Value %.*s\n", (int)rg->nvalue, rg->value);
    } else if (cbtype == LCB_CALLBACK_SDMUTATE || cbtype == LCB_CALLBACK_SDLOOKUP) {
        const lcb_RESPSUBDOC *resp = reinterpret_cast<const lcb_RESPSUBDOC*>(rb);
        lcb_SDENTRY ent;
        size_t iter = 0;
        if (lcb_sdresult_next(resp, &ent, &iter)) {
            fprintf(stderr, "Status: 0x%x. Value: %.*s\n", ent.status, (int)ent.nvalue, ent.value);
        } else {
            fprintf(stderr, "No result!\n");
        }
    } else {
        fprintf(stderr, "OK\n");
    }
}

// cluster_run mode
#define DEFAULT_CONNSTR "couchbase://10.10.4.42/vcmControlSessionDb"
int main(int argc, char **argv)
{
    lcb_create_st crst = { 0 };
    crst.version = 3;
    crst.v.v3.connstr = DEFAULT_CONNSTR;

    lcb_t instance;
    lcb_error_t rc = lcb_create(&instance, &crst);
    assert(rc == LCB_SUCCESS);

    rc = lcb_connect(instance);
    assert(rc == LCB_SUCCESS);

    lcb_wait(instance);

    rc = lcb_get_bootstrap_status(instance);
    assert(rc == LCB_SUCCESS);
/*  SubDoc 
    lcb_install_callback3(instance, LCB_CALLBACK_DEFAULT, op_callback);

    lcb_CMDSUBDOC mcmd = { 0 };
    lcb_CMDSUBDOC cmd = { 0 };
    LCB_CMD_SET_KEY(&mcmd, "Bhushan", 7);

    std::vector<lcb_SDSPEC> specs;
    std::vector<std ::string> path;
    std::vector<std::string> value;

    path.push_back("plmn");
    value.push_back("{\"mcc\":\"555\",\"mnc\":\"11\"}");
    path.push_back("nfServices[0].fqdn");
    value.push_back("\"nssf.mavenirBhushan.com\"");
    int i;
    for (i = 0 ; i < value.size();i++) 
    {
        //for (auto ptr = path.begin() && ptr1 = value.begin(); ptr != path.end() && ptr1 != value.end() ; ptr++,ptr1++) {

        lcb_SDSPEC spec = {0};
        LCB_SDSPEC_SET_PATH(&spec,path[i].c_str(), path[i].length());
        LCB_CMD_SET_VALUE(&spec, value[i].c_str(), value[i].length());
        spec.sdcmd = LCB_SDCMD_DICT_UPSERT;
        specs.push_back(spec);

    }
    mcmd.specs = specs.data();
    mcmd.nspecs = specs.size();
    rc = lcb_subdoc3(instance, NULL, &mcmd);
    assert(rc == LCB_SUCCESS);
    lcb_wait(instance);
*/
/*    printf("Updating the 'plmn' path from the document\n");
    lcb_SDSPEC spec = {0};
    LCB_SDSPEC_SET_PATH(&spec, "plmn", 4);
    const char *initval = "{\"mcc\":\"555\",\"mnc\":\"11\"}";
    LCB_CMD_SET_VALUE(&spec, initval, strlen(initval));
    spec.sdcmd = LCB_SDCMD_DICT_UPSERT;
    specs.push_back(spec);

    printf("Updating the 'nfServices[0].fqdn' path from the document\n");
    LCB_SDSPEC_SET_PATH(&spec, "nfServices[0].fqdn", strlen("nfServices[0].fqdn"));
    initval = "\"nssf.mavenirBgl.com\"";
    LCB_CMD_SET_VALUE(&spec, initval, strlen(initval));
    spec.sdcmd = LCB_SDCMD_DICT_UPSERT;
    specs.push_back(spec);

    printf("Updating the 'nfServices[0].allowedNssais' path from the document\n");
    spec.sdcmd = LCB_SDCMD_DICT_UPSERT;
    LCB_SDSPEC_SET_PATH(&spec, "nfServices[0].allowedNssais", strlen("nfServices[0].allowedNssais"));
    initval = "[{\"sst\":100,\"sd\":\"1\"},{\"sst\":200,\"sd\":\"2\"}]";
    LCB_CMD_SET_VALUE(&spec, initval, strlen(initval));
    spec.sdcmd = LCB_SDCMD_DICT_UPSERT;
    specs.push_back(spec);

    mcmd.specs = specs.data();
    mcmd.nspecs = specs.size();
    rc = lcb_subdoc3(instance, NULL, &mcmd);
    assert(rc == LCB_SUCCESS);
    lcb_wait(instance);

    memset(&spec, 0, sizeof spec);
    memset(&cmd, 0, sizeof cmd);
    printf("Getting first array element...\n");
    LCB_CMD_SET_KEY(&cmd, "nssf",4);
    cmd.specs = &spec;
    cmd.nspecs = 1;

    spec.sdcmd = LCB_SDCMD_GET;
    LCB_SDSPEC_SET_PATH(&spec, "nfServices[0].fqdn", strlen("nfServices[0].fqdn"));
    rc = lcb_subdoc3(instance, NULL, &cmd);
    assert(rc == LCB_SUCCESS);
    lcb_wait(instance);
*/
    lcb_destroy(instance);
    return 0;
}
