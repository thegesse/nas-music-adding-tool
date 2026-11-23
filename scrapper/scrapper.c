#include 
#include 
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

struct CURLResponse {
char *html;
size_t size;
};

typedef struct {
char *url;
char *image;
char *name;
char *price;
} Product;
// change later to fit format

//fetch page
static size_t WriteHTMLCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct CURLResponse *mem = (struct CURLResponse *)userp;

    char *ptr = realloc(mem->html, mem->size   realsize   1);
    if (!ptr) return 0;
    
    mem->html = ptr;
    
    memcpy(&(mem->html[mem->size]), contents, realsize);
    mem->size  = realsize;
    mem->html[mem->size] = 0;
    
    return realsize;
}

struct CURLResponse GetHTML(const char *url) {
    CURL *curl = curl_easy_init();

    struct CURLResponse res = {.html = malloc(1), .size = 0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteHTMLCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&res);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return res;
}
// parse html with libxml2
htmlDocPtr ParseHTML(const char *html, size_t size) {
    return htmlReadMemory(html, size, NULL, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
}

xmlXPathObjectPtr GetNodesByXPath(htmlDocPtr doc, const char *xpathExpr) {
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    return xmlXPathEvalExpression((xmlChar *)xpathExpr, context);
}
// scrap product data
int ExtractProducts(htmlDocPtr doc, Product *products, int startIndex) {
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr items = xmlXPathEvalExpression((xmlChar *)"//li[contains(@class,'product')]", context);

    int count = 0;
    for (int i = 0; i < items->nodesetval->nodeNr;   i) {
        xmlNodePtr node = items->nodesetval->nodeTab[i];
        xmlXPathSetContextNode(node, context);

        char *url = (char *)xmlGetProp(xmlXPathEvalExpression((xmlChar *)".//a", context)->nodesetval->nodeTab[0], (xmlChar *)"href");
        char *img = (char *)xmlGetProp(xmlXPathEvalExpression((xmlChar *)".//a/img", context)->nodesetval->nodeTab[0], (xmlChar *)"src");
        char *name = (char *)xmlNodeGetContent(xmlXPathEvalExpression((xmlChar *)".//a/h2", context)->nodesetval->nodeTab[0]);
        char *price = (char *)xmlNodeGetContent(xmlXPathEvalExpression((xmlChar *)".//a/span", context)->nodesetval->nodeTab[0]);

        products[startIndex   count].url = strdup(url);
        products[startIndex   count].image = strdup(img);
        products[startIndex   count].name = strdup(name);
        products[startIndex   count].price = strdup(price);

        count++;
        free(url); free(img); free(name); free(price);
    }

    xmlXPathFreeContext(context);
    xmlXPathFreeObject(items);
    return count;
}

//export to csv
// change later to fit mp3 format + metadata for navidrome
void ExportToCSV(Product *products, int count) {
    FILE *fp = fopen("products.csv", "w");
    fprintf(fp, "url,image,name,pricen");

    for (int i = 0; i < count; i  ) {
        fprintf(fp, ""%s","%s","%s","%s"n",
        products[i].url, products[i].image, products[i].name, products[i].price);
    }

    fclose(fp);
}