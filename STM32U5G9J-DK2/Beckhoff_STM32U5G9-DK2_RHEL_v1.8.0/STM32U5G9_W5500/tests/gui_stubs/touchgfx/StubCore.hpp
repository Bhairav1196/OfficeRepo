#ifndef TOUCHGFX_STUB_CORE_HPP
#define TOUCHGFX_STUB_CORE_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
namespace touchgfx {
typedef uint32_t colortype;
struct Rect { int16_t x,y,width,height; Rect(int16_t X=0,int16_t Y=0,int16_t W=0,int16_t H=0):x(X),y(Y),width(W),height(H){} };
class Drawable {
public:
    Drawable():invalidations(0),r(),visible(true){}
    virtual ~Drawable(){}
    Rect getAbsoluteRect() const {return r;}
    void setPosition(int16_t x,int16_t y,int16_t w,int16_t h){r=Rect(x,y,w,h);}
    void setVisible(bool v){visible=v;}
    bool isVisible() const {return visible;}
    virtual void invalidate(){++invalidations;}
    unsigned invalidations;
protected:
    Rect r;
    bool visible;
};
class Box: public Drawable { public: Box():c(0){} void setColor(colortype v){c=v;} colortype getColor()const{return c;} private: colortype c; };
class Image: public Drawable { public: Image(){} };
class Button: public Drawable { public: Button(){} };
class TextArea: public Drawable { public: TextArea():c(0),typed(0){} void setColor(colortype value){c=value;} colortype getColor()const{return c;} void setTypedText(int value){typed=value;} int getTypedText()const{return typed;} private:colortype c; int typed; };
class TextAreaWithOneWildcard: public Drawable {
public:
    TextAreaWithOneWildcard():wildcard(0), c(0), typed(0){}
    void setWildcard(const uint16_t* value){wildcard=value;}
    void setWildcard1(const uint16_t* value){wildcard=value;}
    const uint16_t* getWildcard() const {return wildcard;}
    void setColor(colortype value){c=value;}
    colortype getColor() const {return c;}
    void setTypedText(int value){typed=value;}
    int getTypedText() const {return typed;}
private:
    const uint16_t* wildcard;
    colortype c;
    int typed;
};
class Color { public: static colortype getColorFrom24BitRGB(uint8_t r,uint8_t g,uint8_t b){return ((uint32_t)r<<16)|((uint32_t)g<<8)|b;} static colortype getColorFromRGB(uint8_t r,uint8_t g,uint8_t b){return getColorFrom24BitRGB(r,g,b);} };
class Unicode { public: typedef uint16_t UnicodeChar; static int snprintf(UnicodeChar*out,uint16_t n,const char*fmt,...){ if(!out||n==0)return 0; char tmp[128]; va_list ap;va_start(ap,fmt);int rc=vsnprintf(tmp,sizeof(tmp),fmt,ap);va_end(ap);size_t i=0;for(;i+1<n&&tmp[i];++i)out[i]=(uint8_t)tmp[i];out[i]=0;return rc;} static uint16_t fromUTF8(const uint8_t*src,UnicodeChar*dst,uint16_t n){ if(!dst||n==0)return 0; if(!src){dst[0]=0;return 0;} uint16_t i=0;for(;i+1<n&&src[i];++i)dst[i]=src[i];dst[i]=0;return i;} };
class ClickEvent { public: enum ClickEventType {PRESSED, RELEASED}; ClickEvent(ClickEventType t,int16_t x,int16_t y):type(t),px(x),py(y){} ClickEventType getType()const{return type;} int16_t getX()const{return px;} int16_t getY()const{return py;} private: ClickEventType type;int16_t px,py; };
template<class P> class View { public: View():presenter(0){} virtual ~View(){} virtual void setupScreen(){} virtual void tearDownScreen(){} virtual void handleClickEvent(const ClickEvent&){} void bind(P* p){presenter=p;} void add(Drawable&){} void invalidate(){} protected:P* presenter; };
class Presenter { public: virtual ~Presenter(){} virtual void activate(){} virtual void deactivate(){} };
}
#endif
