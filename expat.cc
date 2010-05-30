#include <node.h>
#include <node_events.h>
extern "C" {
#include <expat.h>
}

using namespace v8;
using namespace node;

static Persistent<String> sym_startElement, sym_endElement,
  sym_text, sym_processingInstruction,
  sym_comment, sym_xmlDecl;

class Parser : public EventEmitter {
public:
  static void Initialize(Handle<Object> target)
  {
    HandleScope scope;
    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->Inherit(EventEmitter::constructor_template);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "parse", Parse);
    NODE_SET_PROTOTYPE_METHOD(t, "setEncoding", SetEncoding);

    target->Set(String::NewSymbol("Parser"), t->GetFunction());

    sym_startElement = NODE_PSYMBOL("startElement");
    sym_endElement = NODE_PSYMBOL("endElement");
    sym_text = NODE_PSYMBOL("text");
    sym_processingInstruction = NODE_PSYMBOL("processingInstruction");
    sym_comment = NODE_PSYMBOL("comment");
    sym_xmlDecl = NODE_PSYMBOL("xmlDecl");
  }

protected:
  static Handle<Value> New(const Arguments& args)
  {
    HandleScope scope;
    XML_Char *encoding = NULL;
    if (args.Length() == 1 && args[0]->IsString())
      {
        encoding = new XML_Char[32];
        args[0]->ToString()->WriteAscii(encoding, 0, 32);
      }

    Parser *parser = new Parser(encoding);
    if (encoding)
      delete[] encoding;
    parser->Wrap(args.This());
    return args.This();
  }

  Parser(const XML_Char *encoding)
    : EventEmitter()
  {
    parser = XML_ParserCreate(encoding);
    assert(parser != NULL);

    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, StartElement, EndElement);
    XML_SetCharacterDataHandler(parser, Text);
    XML_SetProcessingInstructionHandler(parser, ProcessingInstruction);
    XML_SetCommentHandler(parser, Comment);
    XML_SetXmlDeclHandler(parser, XmlDecl);
  }

  ~Parser()
  {
    XML_ParserFree(parser);
  }

  static Handle<Value> Parse(const Arguments& args)
  {
    Parser *parser = ObjectWrap::Unwrap<Parser>(args.This());
    HandleScope scope;
    Local<String> str;
    int isFinal = 0;

    /* Argument 1: buf :: String */
    if (args.Length() >= 1 && args[0]->IsString())
      {
        str = args[0]->ToString();
      }
    else
      return scope.Close(False());

    /* Argument 2: isFinal :: Bool */
    if (args.Length() >= 2)
      {
        isFinal = args[1]->IsTrue();
      }

    return scope.Close(parser->parse(**str, isFinal) ? True() : False());
  }

  bool parse(String &str, int isFinal)
  {
    int len = str.Utf8Length();
    void *buf = XML_GetBuffer(parser, len);
    assert(buf != NULL);
    assert(str.WriteUtf8(static_cast<char *>(buf), len) == len);

    return XML_ParseBuffer(parser, len, isFinal) != 0;
  }

  static Handle<Value> SetEncoding(const Arguments& args)
  {
    Parser *parser = ObjectWrap::Unwrap<Parser>(args.This());
    HandleScope scope;

    if (args.Length() == 1 && args[0]->IsString())
      {
        XML_Char *encoding = new XML_Char[32];
        args[0]->ToString()->WriteAscii(encoding, 0, 32);

        int status = parser->setEncoding(encoding);

        delete[] encoding;

        return scope.Close(status ? True() : False());
      }
    else
      return False();
  }

  int setEncoding(XML_Char *encoding)
  {
    return XML_SetEncoding(parser, encoding) != 0;
  }

private:
  /* expat instance */
  XML_Parser parser;

  /* no default ctor */
  Parser();

  /*** SAX callbacks ***/
  /* Should a local HandleScope be used in those callbacks? */

  static void StartElement(void *userData,
                           const XML_Char *name, const XML_Char **atts)
  {
    Parser *parser = reinterpret_cast<Parser *>(userData);

    /* Collect atts into JS object */
    Local<Object> attr = Object::New();
    for(const XML_Char **atts1 = atts; *atts1; atts1 += 2)
      attr->Set(String::New(atts1[0]), String::New(atts1[1]));

    /* Trigger event */
    Handle<Value> argv[2] = { String::New(name), attr };
    parser->Emit(sym_startElement, 2, argv);
  }

  static void EndElement(void *userData,
                         const XML_Char *name)
  {
    Parser *parser = reinterpret_cast<Parser *>(userData);

    /* Trigger event */
    Handle<Value> argv[1] = { String::New(name) };
    parser->Emit(sym_endElement, 1, argv);
  }

  static void Text(void *userData,
                   const XML_Char *s, int len)
  {
    Parser *parser = reinterpret_cast<Parser *>(userData);

    /* Trigger event */
    Handle<Value> argv[1] = { String::New(s, len) };
    parser->Emit(sym_text, 1, argv);
  }

  static void ProcessingInstruction(void *userData,
                                    const XML_Char *target, const XML_Char *data)
  {
    Parser *parser = reinterpret_cast<Parser *>(userData);

    /* Trigger event */
    Handle<Value> argv[2] = { String::New(target), String::New(data) };
    parser->Emit(sym_processingInstruction, 2, argv);
  }

  static void Comment(void *userData,
                      const XML_Char *data)
  {
    Parser *parser = reinterpret_cast<Parser *>(userData);

    /* Trigger event */
    Handle<Value> argv[1] = { String::New(data) };
    parser->Emit(sym_comment, 1, argv);
  }

  static void XmlDecl(void *userData,
                      const XML_Char *version, const XML_Char *encoding,
                      int standalone)
  {
    Parser *parser = reinterpret_cast<Parser *>(userData);

    /* Trigger event */
    Handle<Value> argv[3] = { String::New(version),
                              String::New(encoding),
                              Boolean::New(standalone) };
    parser->Emit(sym_xmlDecl, 3, argv);
  }
};



extern "C" void init(Handle<Object> target)
{
  HandleScope scope;
  Parser::Initialize(target);
}
