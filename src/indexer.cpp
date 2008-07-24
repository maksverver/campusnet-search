#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

#include <errno.h>
#include <time.h>
#include <libgen.h>

#include <libsmbclient.h>
#include <magic.h>
#include <openssl/md5.h>

#define MAX_RECURSION_DEPTH   12
#define DIRENT_BUFFER_SIZE    1024*1024     /* 1 MB */

//
// Type definitions
//

enum node_type_t { host, share, directory, file };

typedef long long int64;
typedef unsigned long long uint64;

struct Digest
{
    Digest();

    Digest &operator ^=(const Digest &d);
    operator bool () const;

    union {
        unsigned char bytes[MD5_DIGEST_LENGTH];
        unsigned ints [MD5_DIGEST_LENGTH/sizeof(unsigned)];
    } data;
};

Digest::Digest()
{
    std::memset(&data, 0, sizeof(data));
}

Digest &Digest::operator ^=(const Digest &d)
{
    for(size_t n = 0; n < MD5_DIGEST_LENGTH/sizeof(unsigned); ++n)
        data.ints[n] ^= d.data.ints[n];
    return *this;
}

Digest::operator bool () const
{
    for(size_t n = 0; n < MD5_DIGEST_LENGTH/sizeof(unsigned); ++n)
        if(data.ints[n])
            return true;
    return false;
}

struct Node
{
    inline Node();

    // Mandatory
    std::string path;
    node_type_t type;

    // Optional
    Node *parent;
    std::string alias;
    std::string error;
    std::string description;
    std::string mime_type;
    std::set<std::string> keywords;
    time_t last_modified;

    // Aggregate
    int64 size;
    int64 files;
    Digest digest;
};

Node::Node() : parent(0), size(-1), files(-1) {
}


//
// Global variables
//

static std::string argv0;
static magic_t magic_text, magic_mime;
static char des_buffer[MAX_RECURSION_DEPTH][DIRENT_BUFFER_SIZE];
static std::ofstream output_stream;
static const char *output_path;


void get_auth_data( const char *srv, const char *shr,
    char *wg, int wglen, char *un, int unlen, char *pw, int pwlen )
{
}

int identify(int fd, off_t filesize, Node &node)
{
    char excerpt[2048];
    ssize_t excerpt_size;
    MD5_CTX mc;
    const char *magic;

    /* Read an excerpt to describe file contents */
    memset(excerpt, 0, sizeof(excerpt));
    excerpt_size = smbc_read(fd, excerpt, sizeof(excerpt));
    if(excerpt_size < 0)
        node.error = strerror(errno);
    smbc_close(fd);
    if(excerpt_size < 0)
        return 1;

    /* Compute file digest */
    MD5_Init(&mc);
    MD5_Update(&mc, (const unsigned char*)&filesize, sizeof(filesize));
    MD5_Update(&mc, (const unsigned char*)excerpt, excerpt_size);
    MD5_Final(node.digest.data.bytes, &mc);

    /* Add file description */
    /* DISABLED FOR NOW
    magic = magic_buffer(magic_text,(const void *)excerpt, excerpt_size);
    if(magic != NULL)
        node.description = magic;
    */

    /* Add mime type */
    magic = magic_buffer(magic_mime, (const void *)excerpt, excerpt_size);
    if(magic != NULL)
        node.mime_type = magic;

    return 0;
}

std::ostream &write_literal(std::ostream &os, const std::string &l)
{
    os << '"';
    for(std::string::const_iterator i = l.begin(); i != l.end(); ++i)
    {
        if(*i == '"')
            os << "\\\"";
        else
        if(*i == '\\')
            os << "\\\\";
        else
            os << *i;
    }
    os << '"';
    return os;
}

std::ostream &write_literal(std::ostream &os, const uint64 &l)
{
    return os << '"' << l << "\"^^xsd:integer";
}

std::ostream &write_literal(std::ostream &os, const int64 &l)
{
    return os << '"' << l << "\"^^xsd:integer";
}

std::ostream &write_literal(std::ostream &os, const Digest &l)
{

    const char hexdigits[] = "0123456789ABCDEF";
    os << '"';
    for(size_t n = 0; n < sizeof(l.data.bytes); ++n)
        os << hexdigits[l.data.bytes[n]/16] << hexdigits[l.data.bytes[n]%16];
    os << '"';
    return os;
}

std::ostream &write_literal(std::ostream &os, const time_t &t)
{
    char buffer[32];
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
    return os << '"' << buffer << "\"^^xsd:dateTime";
}

void output(const Node &node)
{
    if(!output_stream.is_open())
    {
        output_stream.open(output_path);
        if(!output_stream)
        {
            std::cerr << "Unable to open output file " << output_path
                      << " for writing!" << std::endl;
            std::exit(1);
        }

        output_stream <<
            "@prefix :     <http://maks.student.utwente.nl/ns/smb#> .\n"
            "@prefix xsd:  <http://www.w3.org/2001/XMLSchema#> .\n";
    }

    if(node.parent)
        output_stream << "<" << node.parent->path << "> :contains <" << node.path << "> .\n";

    const char *type_name[] = { "host", "share", "directory", "file" };

    write_literal(output_stream << "<" << node.path << ">\n\t:type ", type_name[node.type]);

    if(!node.error.empty())
        write_literal(output_stream << " ;\n\t:error ", node.error);

    if(!node.description.empty())
        write_literal(output_stream << " ;\n\t:description ", node.description);

    if(!node.mime_type.empty())
        write_literal(output_stream << " ;\n\t:mediaType ", node.mime_type);

    if(!node.keywords.empty())
    {
        std::set<std::string>::const_iterator i = node.keywords.begin();
        write_literal(output_stream << " ;\n\t:keyword ", *i);
        while(++i != node.keywords.end())
            write_literal(output_stream << ", ", *i);
    }

    if(node.size >= 0)
        write_literal(output_stream << " ;\n\t:size ", node.size);

    if(node.files >= 0)
        write_literal(output_stream << " ;\n\t:files ", node.files);

    if(node.digest)
        write_literal(output_stream << " ;\n\t:digest ", node.digest);

    if(node.last_modified != 0)
        write_literal(output_stream << " ;\n\t:lastModified ", node.last_modified);

    output_stream << " .\n";
}


/* Urldecodes path and splits it into keywords. */
void path_keywords(const char *path, std::set<std::string> &keywords)
{
    char buffer[512], *i, *j;
    smbc_urldecode(buffer, (char*)path, 512);
    i = buffer, j = buffer - 1;
    do {
        ++j;
        if(!isalnum(*j))
        {
            if(i != j)
            {
                std::string s(i, j);
                for(std::string::iterator it = s.begin(); it != s.end(); ++it)
                    *it = std::tolower(*it);
                keywords.insert(s);
            }
            i = j + 1;
        }
    } while(*j);
}

bool browse(Node &node, int depth = 1)
{
    struct stat st;
    if(node.type != host)
    {
        if(smbc_stat(node.path.c_str(), &st) != 0)
        {
            node.error = strerror(errno);
            return false;
        }

        node.last_modified = st.st_mtime;
    }

    if(node.type == file)
    {
        int fd;
        if(st.st_size > 0 && (fd = smbc_open(node.path.c_str(), O_RDONLY, 0)) >= 0)
        {
            node.size = st.st_size;
            if(identify(fd, st.st_size, node) != 0)
                std::cerr << argv0 << ": failed to identify file " << node.path << std::endl;
        }
    }
    else
    if(depth < MAX_RECURSION_DEPTH)
    {
        // Read entries in directory
        int dh = smbc_opendir(node.path.c_str());
        if(dh < 0)
        {
            node.error = strerror(errno);
            return false;
        }
    
        struct smbc_dirent *des = (struct smbc_dirent*)des_buffer[depth-1];
        int de_count = smbc_getdents(dh, des, DIRENT_BUFFER_SIZE);
        smbc_closedir(dh);
        if(de_count < 0)
        {
            node.error = strerror(errno);
            return false;
        }
    
        for( struct smbc_dirent *de = des; de_count > 0;
            de_count -= de->dirlen, de = (struct smbc_dirent*)(((char*)de) + de->dirlen) )
        {
            /* Skip entries beginning with a dot (including . and ..) */
            if(de->name[0] == '.')
                continue;
    
            /* Add entry to path */
            Node child;
            child.path     = node.path + '/' + de->name;
            child.parent   = &node;

            /* Set keywords */
            child.keywords = node.keywords;
            path_keywords(de->name, child.keywords);

            /* Browse */
            switch(de->smbc_type)
            {
            case SMBC_FILE_SHARE:
            case SMBC_DIR:
                {
                    child.type = ((de->smbc_type == SMBC_FILE_SHARE) ? share : directory);
                    if(de->commentlen > 1)
                        child.description = de->comment;

                    child.size  = 0;
                    child.files = 0;

                    browse(child, depth + 1);

                    node.files  += child.files;
                    node.size   += child.size;
                    node.digest ^= child.digest;
                }
                break;

            case SMBC_FILE:
                {
                    child.type  = file;

                    browse(child, depth + 1);

                    node.files += 1;
                    node.size  += child.size;
                    node.digest ^= child.digest;
                }
                break;

            case SMBC_PRINTER_SHARE:
            case SMBC_COMMS_SHARE:
            case SMBC_IPC_SHARE:
            case SMBC_LINK:
                {
                    /* Skipped. */
                }
                break;

            default:
                {
                    /* Unrecognized type! */
                    std::cerr << argv0 << ": unrecognized node type at " << node.path << std::endl;
                }
                break;
            }

            // Output data
            output(child);
        }
    }

    return true;
}

int main(int argc, char *argv[])
{
    argv0 = basename(argv[0]);

    if(argc != 3)
    {
        std::cerr << "Usage: " << argv0 << " <host> <path>" << std::endl;
        return 0;
    }

    int result = 0;
    const char *host_name = argv[1];
    output_path = argv[2];

    // Initialize libsmbclient
    if(smbc_init(get_auth_data, 0) != 0)
    {
        std::cerr << argv0 << ": failed to initialize SMB client library" << std::endl;
        return 1;
    }

    // Open magic file identification databases
    if( (magic_text = magic_open(MAGIC_NONE)) == NULL ||
        magic_load(magic_text, NULL) < 0 )
    {
        std::cerr << argv0 << ": failed to load magic text database\n" << std::endl;
        return 1;
    }

    if( (magic_mime = magic_open(MAGIC_MIME)) == NULL ||
        magic_load(magic_mime, NULL) < 0 )
    {
        magic_close(magic_text);
        std::cerr << argv0 << ": failed to load magic mime database\n" << std::endl;
        return 1;
    }

    // Initialize SMB context
    SMBCCTX *ctx = smbc_set_context(NULL);
    ctx->timeout = 2000; /* ms */
    ctx->options.urlencode_readdir_entries = 1;
    smbc_set_context(ctx);

    // Index host recursively
    Node root;
    root.path  = std::string("smb://") + host_name;
    root.type  = host;
    root.files = 0;
    root.size  = 0;
    path_keywords(host_name, root.keywords);
    if(browse(root))
    {
        output(root);
        write_literal(output_stream << "<" << root.path << "> :lastIndexed ", time(NULL)) << " .\n";
    }
    else
    {
        std::cerr << argv0 << ": unable to index host " << host_name << std::endl;
        result = 1;
    }

    // Clean-up
    magic_close(magic_text);
    magic_close(magic_mime);

    return result;
}
