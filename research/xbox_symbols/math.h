#ifndef _MATH_H_
#define _MATH_H_

// Category: math
// Extracted from Xbox PDB symbols
// Total types: 37
// Note: Xbox symbols - may need adaptation for GameCube

struct _VECTOR4// Size=0x10 (Id=726)
{
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct CRYPTO_VECTOR// Size=0x40 (Id=1906)
{
    void  ( * pXcSHAInit)(unsigned char * );// Offset=0x0 Size=0x4
    void  ( * pXcSHAUpdate)(unsigned char * ,unsigned char * ,unsigned long );// Offset=0x4 Size=0x4
    void  ( * pXcSHAFinal)(unsigned char * ,unsigned char * );// Offset=0x8 Size=0x4
    void  ( * pXcRC4Key)(unsigned char * ,unsigned long ,unsigned char * );// Offset=0xc Size=0x4
    void  ( * pXcRC4Crypt)(unsigned char * ,unsigned long ,unsigned char * );// Offset=0x10 Size=0x4
    void  ( * pXcHMAC)(unsigned char * ,unsigned long ,unsigned char * ,unsigned long ,unsigned char * ,unsigned long ,unsigned char * );// Offset=0x14 Size=0x4
    unsigned long  ( * pXcPKEncPublic)(unsigned char * ,unsigned char * ,unsigned char * );// Offset=0x18 Size=0x4
    unsigned long  ( * pXcPKDecPrivate)(unsigned char * ,unsigned char * ,unsigned char * );// Offset=0x1c Size=0x4
    unsigned long  ( * pXcPKGetKeyLen)(unsigned char * );// Offset=0x20 Size=0x4
    unsigned char  ( * pXcVerifyPKCS1Signature)(unsigned char * ,unsigned char * ,unsigned char * );// Offset=0x24 Size=0x4
    unsigned long  ( * pXcModExp)(unsigned long * ,unsigned long * ,unsigned long * ,unsigned long * ,unsigned long );// Offset=0x28 Size=0x4
    void  ( * pXcDESKeyParity)(unsigned char * ,unsigned long );// Offset=0x2c Size=0x4
    void  ( * pXcKeyTable)(unsigned long ,unsigned char * ,unsigned char * );// Offset=0x30 Size=0x4
    void  ( * pXcBlockCrypt)(unsigned long ,unsigned char * ,unsigned char * ,unsigned char * ,unsigned long );// Offset=0x34 Size=0x4
    void  ( * pXcBlockCryptCBC)(unsigned long ,unsigned long ,unsigned char * ,unsigned char * ,unsigned char * ,unsigned long ,unsigned char * );// Offset=0x38 Size=0x4
    unsigned long  ( * pXcCryptService)(unsigned long ,void * );// Offset=0x3c Size=0x4
};

class vec3 : public vec3_s// Size=0xc (Id=2015)
{
    union // Size=0x54 (Id=0)
    {
        void vec3(class vec3 & );// Offset=0x0 Size=0x19
        void vec3(class vec2 & );// Offset=0x0 Size=0x13
        void vec3(float ,float );// Offset=0x0 Size=0x12
        void vec3(float ,float ,float );// Offset=0x0 Size=0x19
        void vec3();// Offset=0x0 Size=0x3
        float length_f32();// Offset=0x0 Size=0x21
        float lengthSquared_f32();// Offset=0x0 Size=0x1f
        class vec3 & normalize();// Offset=0x0 Size=0x54
        class vec3 & operator=(class vec2 & );// Offset=0x0 Size=0x13
        class vec3 & operator+=(class vec3 & );// Offset=0x0 Size=0x21
        class vec3 & operator-=(class vec3 & );// Offset=0x0 Size=0x21
        class vec3 & operator*=(float );// Offset=0x0 Size=0x21
        class vec3 & operator/=(class vec3 & );// Offset=0x0 Size=0x21
        class vec3 & operator/=(float );// Offset=0x0 Size=0x23
    };
};

class vec2 : public vec2_s// Size=0x8 (Id=2016)
{
    union // Size=0x4e (Id=0)
    {
        void vec2(class vec2 & );// Offset=0x0 Size=0x13
        void vec2(float ,float );// Offset=0x0 Size=0x12
        void vec2();// Offset=0x0 Size=0x3
        float length_f32();// Offset=0x0 Size=0x16
        float lengthSquared_f32();// Offset=0x0 Size=0x14
        class vec2 & normalize();// Offset=0x0 Size=0x4e
        class vec2 & operator+=(class vec2 & );// Offset=0x0 Size=0x18
        class vec2 & operator-=(class vec2 & );// Offset=0x0 Size=0x18
        class vec2 & operator*=(float );// Offset=0x0 Size=0x17
        class vec2 & operator/=(float );// Offset=0x0 Size=0x1b
    };
};

class vec3 : public vec3_s// Size=0xc (Id=2017)
{
    union // Size=0x54 (Id=0)
    {
        void vec3(class vec3 & );// Offset=0x0 Size=0x19
        void vec3(class vec2 & );// Offset=0x0 Size=0x13
        void vec3(float ,float );// Offset=0x0 Size=0x12
        void vec3(float ,float ,float );// Offset=0x0 Size=0x19
        void vec3();// Offset=0x0 Size=0x3
        float length_f32();// Offset=0x0 Size=0x21
        float lengthSquared_f32();// Offset=0x0 Size=0x1f
        class vec3 & normalize();// Offset=0x0 Size=0x54
        class vec3 & operator=(class vec2 & );// Offset=0x0 Size=0x13
        class vec3 & operator+=(class vec3 & );// Offset=0x0 Size=0x21
        class vec3 & operator-=(class vec3 & );// Offset=0x0 Size=0x21
        class vec3 & operator*=(float );// Offset=0x0 Size=0x21
        class vec3 & operator/=(class vec3 & );// Offset=0x0 Size=0x21
        class vec3 & operator/=(float );// Offset=0x0 Size=0x23
    };
};

class vec4 : public vec4_s// Size=0x10 (Id=2021)
{
    union // Size=0x67 (Id=0)
    {
        void vec4(class vec2 & );// Offset=0x0 Size=0x13
        void vec4(class vec3 & );// Offset=0x0 Size=0x19
        void vec4(class vec4 & );// Offset=0x0 Size=0x1f
        void vec4(float ,float );// Offset=0x0 Size=0x12
        void vec4(float ,float ,float );// Offset=0x0 Size=0x19
        void vec4(float ,float ,float ,float );// Offset=0x0 Size=0x20
        void vec4();// Offset=0x0 Size=0x3
        float length_f32();// Offset=0x0 Size=0x2c
        float lengthSquared_f32();// Offset=0x0 Size=0x2a
        class vec4 & normalize();// Offset=0x0 Size=0x67
        class vec4 & operator=(class vec2 & );// Offset=0x0 Size=0x13
        class vec4 & operator=(class vec3 & );// Offset=0x0 Size=0x19
        class vec4 & operator+=(class vec4 & );// Offset=0x0 Size=0x2a
        class vec4 & operator-=(class vec4 & );// Offset=0x0 Size=0x2a
        class vec4 & operator*=(float );// Offset=0x0 Size=0x2b
        class vec4 & operator/=(class vec4 & );// Offset=0x0 Size=0x2a
        class vec4 & operator/=(float );// Offset=0x0 Size=0x2b
    };
};

class vec4 : public vec4_s// Size=0x10 (Id=2022)
{
    union // Size=0x67 (Id=0)
    {
        void vec4(class vec2 & );// Offset=0x0 Size=0x13
        void vec4(class vec3 & );// Offset=0x0 Size=0x19
        void vec4(class vec4 & );// Offset=0x0 Size=0x1f
        void vec4(float ,float );// Offset=0x0 Size=0x12
        void vec4(float ,float ,float );// Offset=0x0 Size=0x19
        void vec4(float ,float ,float ,float );// Offset=0x0 Size=0x20
        void vec4();// Offset=0x0 Size=0x3
        float length_f32();// Offset=0x0 Size=0x2c
        float lengthSquared_f32();// Offset=0x0 Size=0x2a
        class vec4 & normalize();// Offset=0x0 Size=0x67
        class vec4 & operator=(class vec2 & );// Offset=0x0 Size=0x13
        class vec4 & operator=(class vec3 & );// Offset=0x0 Size=0x19
        class vec4 & operator+=(class vec4 & );// Offset=0x0 Size=0x2a
        class vec4 & operator-=(class vec4 & );// Offset=0x0 Size=0x2a
        class vec4 & operator*=(float );// Offset=0x0 Size=0x2b
        class vec4 & operator/=(class vec4 & );// Offset=0x0 Size=0x2a
        class vec4 & operator/=(float );// Offset=0x0 Size=0x2b
    };
};

class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > : public std::_Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >// Size=0x10 (Id=2023)
{
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(unsigned int ,class std::vector<int,std::allocator<int> > & ,class std::allocator<std::vector<int,std::allocator<int> > > & );
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(unsigned int ,class std::vector<int,std::allocator<int> > & );
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(unsigned int );
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(class std::allocator<std::vector<int,std::allocator<int> > > & );
    union // Size=0x2e4 (Id=0)
    {
        void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >();// Offset=0x0 Size=0xe
        void _Construct_n(unsigned int ,class std::vector<int,std::allocator<int> > & );
        void ~vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >();// Offset=0x0 Size=0x3f
        void reserve(unsigned int );
        unsigned int capacity();// Offset=0x0 Size=0x13
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> begin();
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> begin();// Offset=0x0 Size=0xc
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> end();
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> end();// Offset=0x0 Size=0xc
        class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> > rbegin();
        class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> > rbegin();
        class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> > rend();
        class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> > rend();
        void resize(unsigned int ,class std::vector<int,std::allocator<int> > );// Offset=0x0 Size=0xbf
        void resize(unsigned int );// Offset=0x0 Size=0x23
        unsigned int size();// Offset=0x0 Size=0x13
        unsigned int max_size();// Offset=0x0 Size=0x6
        char empty();
        class std::allocator<std::vector<int,std::allocator<int> > > get_allocator();
        class std::vector<int,std::allocator<int> > & at(unsigned int );
        class std::vector<int,std::allocator<int> > & at(unsigned int );
        class std::vector<int,std::allocator<int> > & operator[](unsigned int );
        class std::vector<int,std::allocator<int> > & operator[](unsigned int );// Offset=0x0 Size=0xf
        class std::vector<int,std::allocator<int> > & front();
        class std::vector<int,std::allocator<int> > & front();
        class std::vector<int,std::allocator<int> > & back();
        class std::vector<int,std::allocator<int> > & back();
        void push_back(class std::vector<int,std::allocator<int> > & );
        void pop_back();
        void assign(unsigned int ,class std::vector<int,std::allocator<int> > & );
        void insert(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> ,unsigned int ,class std::vector<int,std::allocator<int> > & );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> insert(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> ,class std::vector<int,std::allocator<int> > & );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> erase(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> ,class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> );// Offset=0x0 Size=0x4d
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> erase(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> );
        void clear();
        char _Eq(class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
        char _Lt(class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
        void swap(class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
        void _Assign_n(unsigned int ,class std::vector<int,std::allocator<int> > & );
        char _Buy(unsigned int );// Offset=0x0 Size=0x51
        void _Destroy(class std::vector<int,std::allocator<int> > * ,class std::vector<int,std::allocator<int> > * );// Offset=0x0 Size=0x1b
        void _Tidy();// Offset=0x0 Size=0x3f
        void _Insert_n(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> ,unsigned int ,class std::vector<int,std::allocator<int> > & );// Offset=0x0 Size=0x2e4
        class std::vector<int,std::allocator<int> > * _Ufill(class std::vector<int,std::allocator<int> > * ,unsigned int ,class std::vector<int,std::allocator<int> > & );// Offset=0x0 Size=0x2b
        void _Xlen();// Offset=0x0 Size=0x7b
        void _Xran();
        class std::vector<int,std::allocator<int> > * _Myfirst;// Offset=0x4 Size=0x4
        class std::vector<int,std::allocator<int> > * _Mylast;// Offset=0x8 Size=0x4
        class std::vector<int,std::allocator<int> > * _Myend;// Offset=0xc Size=0x4
        void * __vecDelDtor(unsigned int );
    };
};

class std::vector<int,std::allocator<int> > : public std::_Vector_val<int,std::allocator<int> >// Size=0x10 (Id=2024)
{
    union // Size=0x29d (Id=0)
    {
        void vector<int,std::allocator<int> >(class std::vector<int,std::allocator<int> > & );// Offset=0x0 Size=0x92
        void vector<int,std::allocator<int> >(unsigned int ,int & ,class std::allocator<int> & );
        void vector<int,std::allocator<int> >(unsigned int ,int & );
        void vector<int,std::allocator<int> >(unsigned int );
        void vector<int,std::allocator<int> >(class std::allocator<int> & );
        void vector<int,std::allocator<int> >();// Offset=0x0 Size=0xe
        void _Construct_n(unsigned int ,int & );
        void ~vector<int,std::allocator<int> >();// Offset=0x0 Size=0x2a
        void reserve(unsigned int );
        unsigned int capacity();// Offset=0x0 Size=0x13
        class std::_Ptrit<int,int,int const *,int const &,int *,int &> begin();// Offset=0x0 Size=0xc
        class std::_Ptrit<int,int,int *,int &,int *,int &> begin();// Offset=0x0 Size=0xc
        class std::_Ptrit<int,int,int const *,int const &,int *,int &> end();// Offset=0x0 Size=0xc
        class std::_Ptrit<int,int,int *,int &,int *,int &> end();// Offset=0x0 Size=0xc
        class std::reverse_iterator<std::_Ptrit<int,int,int const *,int const &,int *,int &> > rbegin();
        class std::reverse_iterator<std::_Ptrit<int,int,int *,int &,int *,int &> > rbegin();
        class std::reverse_iterator<std::_Ptrit<int,int,int const *,int const &,int *,int &> > rend();
        class std::reverse_iterator<std::_Ptrit<int,int,int *,int &,int *,int &> > rend();
        void resize(unsigned int ,int );// Offset=0x0 Size=0x78
        void resize(unsigned int );// Offset=0x0 Size=0xf
        unsigned int size();// Offset=0x0 Size=0x13
        unsigned int max_size();// Offset=0x0 Size=0x6
        char empty();
        class std::allocator<int> get_allocator();
        int & at(unsigned int );
        int & at(unsigned int );
        int & operator[](unsigned int );
        int & operator[](unsigned int );// Offset=0x0 Size=0xd
        int & front();
        int & front();
        int & back();
        int & back();
        void push_back(int & );// Offset=0x0 Size=0x66
        void pop_back();
        void assign(unsigned int ,int & );
        void insert(class std::_Ptrit<int,int,int *,int &,int *,int &> ,unsigned int ,int & );
        class std::_Ptrit<int,int,int *,int &,int *,int &> insert(class std::_Ptrit<int,int,int *,int &,int *,int &> ,int & );// Offset=0x0 Size=0x4a
        class std::_Ptrit<int,int,int *,int &,int *,int &> erase(class std::_Ptrit<int,int,int *,int &,int *,int &> ,class std::_Ptrit<int,int,int *,int &,int *,int &> );// Offset=0x0 Size=0x35
        class std::_Ptrit<int,int,int *,int &,int *,int &> erase(class std::_Ptrit<int,int,int *,int &,int *,int &> );
        void clear();// Offset=0x0 Size=0x2a
        char _Eq(class std::vector<int,std::allocator<int> > & );
        char _Lt(class std::vector<int,std::allocator<int> > & );
        void swap(class std::vector<int,std::allocator<int> > & );
        void _Assign_n(unsigned int ,int & );
        char _Buy(unsigned int );// Offset=0x0 Size=0x53
        void _Destroy(int * ,int * );// Offset=0x0 Size=0x3
        void _Tidy();// Offset=0x0 Size=0x2a
        void _Insert_n(class std::_Ptrit<int,int,int *,int &,int *,int &> ,unsigned int ,int & );// Offset=0x0 Size=0x29d
        int * _Ufill(int * ,unsigned int ,int & );// Offset=0x0 Size=0x27
        void _Xlen();// Offset=0x0 Size=0x7b
        void _Xran();
        int * _Myfirst;// Offset=0x4 Size=0x4
        int * _Mylast;// Offset=0x8 Size=0x4
        int * _Myend;// Offset=0xc Size=0x4
        void * __vecDelDtor(unsigned int );
    };
};

class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> : public std::_Ranit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>// Size=0x4 (Id=2027)
{
    public void _Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
    public void _Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>(class std::vector<int,std::allocator<int> > * );
    public void _Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>();
    union // Size=0x16 (Id=0)
    {
        class std::vector<int,std::allocator<int> > * base();// Offset=0x0 Size=0x3
        class std::vector<int,std::allocator<int> > & operator*();// Offset=0x0 Size=0x3
        class std::vector<int,std::allocator<int> > * operator->();
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> operator++(int );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & operator++();// Offset=0x0 Size=0x6
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> operator--(int );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & operator--();// Offset=0x0 Size=0x6
        char operator==(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );// Offset=0x0 Size=0x10
        char operator==(int );
        char operator!=(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );// Offset=0x0 Size=0x16
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & operator+=(int );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> operator+(int );// Offset=0x0 Size=0x14
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & operator-=(int );
        int operator-(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );// Offset=0x0 Size=0xe
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> operator-(int );// Offset=0x0 Size=0x14
        class std::vector<int,std::allocator<int> > & operator[](int );
        char operator<(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
        char operator>(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
        char operator<=(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
        char operator>=(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
        class std::vector<int,std::allocator<int> > * current;// Offset=0x0 Size=0x4
    };
};

class std::allocator<std::vector<int,std::allocator<int> > >// Size=0x1 (Id=2028)
{
    public class std::vector<int,std::allocator<int> > * address(class std::vector<int,std::allocator<int> > & );
    public class std::vector<int,std::allocator<int> > * address(class std::vector<int,std::allocator<int> > & );
    union // Size=0x2e (Id=0)
    {
        void allocator<std::vector<int,std::allocator<int> > >(class std::allocator<std::vector<int,std::allocator<int> > > & );// Offset=0x0 Size=0x5
        void allocator<std::vector<int,std::allocator<int> > >();// Offset=0x0 Size=0x3
        class std::vector<int,std::allocator<int> > * allocate(unsigned int ,void * );// Offset=0x0 Size=0x13
        class std::vector<int,std::allocator<int> > * allocate(unsigned int );// Offset=0x0 Size=0x13
        void deallocate(class std::vector<int,std::allocator<int> > * ,unsigned int );// Offset=0x0 Size=0xe
        void construct(class std::vector<int,std::allocator<int> > * ,class std::vector<int,std::allocator<int> > & );// Offset=0x0 Size=0x15
        void destroy(class std::vector<int,std::allocator<int> > * );// Offset=0x0 Size=0x2e
        unsigned int max_size();// Offset=0x0 Size=0x6
    };
};

class std::allocator<std::vector<int,std::allocator<int> > >// Size=0x1 (Id=2036)
{
    public class std::vector<int,std::allocator<int> > * address(class std::vector<int,std::allocator<int> > & );
    public class std::vector<int,std::allocator<int> > * address(class std::vector<int,std::allocator<int> > & );
    union // Size=0x2e (Id=0)
    {
        void allocator<std::vector<int,std::allocator<int> > >(class std::allocator<std::vector<int,std::allocator<int> > > & );// Offset=0x0 Size=0x5
        void allocator<std::vector<int,std::allocator<int> > >();// Offset=0x0 Size=0x3
        class std::vector<int,std::allocator<int> > * allocate(unsigned int ,void * );// Offset=0x0 Size=0x13
        class std::vector<int,std::allocator<int> > * allocate(unsigned int );// Offset=0x0 Size=0x13
        void deallocate(class std::vector<int,std::allocator<int> > * ,unsigned int );// Offset=0x0 Size=0xe
        void construct(class std::vector<int,std::allocator<int> > * ,class std::vector<int,std::allocator<int> > & );// Offset=0x0 Size=0x15
        void destroy(class std::vector<int,std::allocator<int> > * );// Offset=0x0 Size=0x2e
        unsigned int max_size();// Offset=0x0 Size=0x6
    };
};

class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> : public std::_Ranit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>// Size=0x4 (Id=2042)
{
    public void _Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
    public void _Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>(class std::vector<int,std::allocator<int> > * );
    public void _Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>();
    union // Size=0x16 (Id=0)
    {
        class std::vector<int,std::allocator<int> > * base();// Offset=0x0 Size=0x3
        class std::vector<int,std::allocator<int> > & operator*();// Offset=0x0 Size=0x3
        class std::vector<int,std::allocator<int> > * operator->();
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> operator++(int );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & operator++();// Offset=0x0 Size=0x6
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> operator--(int );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & operator--();// Offset=0x0 Size=0x6
        char operator==(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );// Offset=0x0 Size=0x10
        char operator==(int );
        char operator!=(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );// Offset=0x0 Size=0x16
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & operator+=(int );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> operator+(int );// Offset=0x0 Size=0x14
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & operator-=(int );
        int operator-(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );// Offset=0x0 Size=0xe
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> operator-(int );// Offset=0x0 Size=0x14
        class std::vector<int,std::allocator<int> > & operator[](int );
        char operator<(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
        char operator>(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
        char operator<=(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
        char operator>=(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> & );
        class std::vector<int,std::allocator<int> > * current;// Offset=0x0 Size=0x4
    };
};

class std::vector<int,std::allocator<int> > : public std::_Vector_val<int,std::allocator<int> >// Size=0x10 (Id=2045)
{
    union // Size=0x29d (Id=0)
    {
        void vector<int,std::allocator<int> >(class std::vector<int,std::allocator<int> > & );// Offset=0x0 Size=0x92
        void vector<int,std::allocator<int> >(unsigned int ,int & ,class std::allocator<int> & );
        void vector<int,std::allocator<int> >(unsigned int ,int & );
        void vector<int,std::allocator<int> >(unsigned int );
        void vector<int,std::allocator<int> >(class std::allocator<int> & );
        void vector<int,std::allocator<int> >();// Offset=0x0 Size=0xe
        void _Construct_n(unsigned int ,int & );
        void ~vector<int,std::allocator<int> >();// Offset=0x0 Size=0x2a
        void reserve(unsigned int );
        unsigned int capacity();// Offset=0x0 Size=0x13
        class std::_Ptrit<int,int,int const *,int const &,int *,int &> begin();// Offset=0x0 Size=0xc
        class std::_Ptrit<int,int,int *,int &,int *,int &> begin();// Offset=0x0 Size=0xc
        class std::_Ptrit<int,int,int const *,int const &,int *,int &> end();// Offset=0x0 Size=0xc
        class std::_Ptrit<int,int,int *,int &,int *,int &> end();// Offset=0x0 Size=0xc
        class std::reverse_iterator<std::_Ptrit<int,int,int const *,int const &,int *,int &> > rbegin();
        class std::reverse_iterator<std::_Ptrit<int,int,int *,int &,int *,int &> > rbegin();
        class std::reverse_iterator<std::_Ptrit<int,int,int const *,int const &,int *,int &> > rend();
        class std::reverse_iterator<std::_Ptrit<int,int,int *,int &,int *,int &> > rend();
        void resize(unsigned int ,int );// Offset=0x0 Size=0x78
        void resize(unsigned int );// Offset=0x0 Size=0xf
        unsigned int size();// Offset=0x0 Size=0x13
        unsigned int max_size();// Offset=0x0 Size=0x6
        char empty();
        class std::allocator<int> get_allocator();
        int & at(unsigned int );
        int & at(unsigned int );
        int & operator[](unsigned int );
        int & operator[](unsigned int );// Offset=0x0 Size=0xd
        int & front();
        int & front();
        int & back();
        int & back();
        void push_back(int & );// Offset=0x0 Size=0x66
        void pop_back();
        void assign(unsigned int ,int & );
        void insert(class std::_Ptrit<int,int,int *,int &,int *,int &> ,unsigned int ,int & );
        class std::_Ptrit<int,int,int *,int &,int *,int &> insert(class std::_Ptrit<int,int,int *,int &,int *,int &> ,int & );// Offset=0x0 Size=0x4a
        class std::_Ptrit<int,int,int *,int &,int *,int &> erase(class std::_Ptrit<int,int,int *,int &,int *,int &> ,class std::_Ptrit<int,int,int *,int &,int *,int &> );// Offset=0x0 Size=0x35
        class std::_Ptrit<int,int,int *,int &,int *,int &> erase(class std::_Ptrit<int,int,int *,int &,int *,int &> );
        void clear();// Offset=0x0 Size=0x2a
        char _Eq(class std::vector<int,std::allocator<int> > & );
        char _Lt(class std::vector<int,std::allocator<int> > & );
        void swap(class std::vector<int,std::allocator<int> > & );
        void _Assign_n(unsigned int ,int & );
        char _Buy(unsigned int );// Offset=0x0 Size=0x53
        void _Destroy(int * ,int * );// Offset=0x0 Size=0x3
        void _Tidy();// Offset=0x0 Size=0x2a
        void _Insert_n(class std::_Ptrit<int,int,int *,int &,int *,int &> ,unsigned int ,int & );// Offset=0x0 Size=0x29d
        int * _Ufill(int * ,unsigned int ,int & );// Offset=0x0 Size=0x27
        void _Xlen();// Offset=0x0 Size=0x7b
        void _Xran();
        int * _Myfirst;// Offset=0x4 Size=0x4
        int * _Mylast;// Offset=0x8 Size=0x4
        int * _Myend;// Offset=0xc Size=0x4
        void * __vecDelDtor(unsigned int );
    };
};

class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>// Size=0x0 (Id=2046)
{
};

class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> >// Size=0x0 (Id=2047)
{
};

class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> >// Size=0x0 (Id=2048)
{
};

class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > : public std::_Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >// Size=0x10 (Id=2049)
{
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(unsigned int ,class std::vector<int,std::allocator<int> > & ,class std::allocator<std::vector<int,std::allocator<int> > > & );
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(unsigned int ,class std::vector<int,std::allocator<int> > & );
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(unsigned int );
    public void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(class std::allocator<std::vector<int,std::allocator<int> > > & );
    union // Size=0x2e4 (Id=0)
    {
        void vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >();// Offset=0x0 Size=0xe
        void _Construct_n(unsigned int ,class std::vector<int,std::allocator<int> > & );
        void ~vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >();// Offset=0x0 Size=0x3f
        void reserve(unsigned int );
        unsigned int capacity();// Offset=0x0 Size=0x13
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> begin();
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> begin();// Offset=0x0 Size=0xc
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> end();
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> end();// Offset=0x0 Size=0xc
        class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> > rbegin();
        class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> > rbegin();
        class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > const *,std::vector<int,std::allocator<int> > const &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> > rend();
        class std::reverse_iterator<std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> > rend();
        void resize(unsigned int ,class std::vector<int,std::allocator<int> > );// Offset=0x0 Size=0xbf
        void resize(unsigned int );// Offset=0x0 Size=0x23
        unsigned int size();// Offset=0x0 Size=0x13
        unsigned int max_size();// Offset=0x0 Size=0x6
        char empty();
        class std::allocator<std::vector<int,std::allocator<int> > > get_allocator();
        class std::vector<int,std::allocator<int> > & at(unsigned int );
        class std::vector<int,std::allocator<int> > & at(unsigned int );
        class std::vector<int,std::allocator<int> > & operator[](unsigned int );
        class std::vector<int,std::allocator<int> > & operator[](unsigned int );// Offset=0x0 Size=0xf
        class std::vector<int,std::allocator<int> > & front();
        class std::vector<int,std::allocator<int> > & front();
        class std::vector<int,std::allocator<int> > & back();
        class std::vector<int,std::allocator<int> > & back();
        void push_back(class std::vector<int,std::allocator<int> > & );
        void pop_back();
        void assign(unsigned int ,class std::vector<int,std::allocator<int> > & );
        void insert(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> ,unsigned int ,class std::vector<int,std::allocator<int> > & );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> insert(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> ,class std::vector<int,std::allocator<int> > & );
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> erase(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> ,class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> );// Offset=0x0 Size=0x4d
        class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> erase(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> );
        void clear();
        char _Eq(class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
        char _Lt(class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
        void swap(class std::vector<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
        void _Assign_n(unsigned int ,class std::vector<int,std::allocator<int> > & );
        char _Buy(unsigned int );// Offset=0x0 Size=0x51
        void _Destroy(class std::vector<int,std::allocator<int> > * ,class std::vector<int,std::allocator<int> > * );// Offset=0x0 Size=0x1b
        void _Tidy();// Offset=0x0 Size=0x3f
        void _Insert_n(class std::_Ptrit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> ,unsigned int ,class std::vector<int,std::allocator<int> > & );// Offset=0x0 Size=0x2e4
        class std::vector<int,std::allocator<int> > * _Ufill(class std::vector<int,std::allocator<int> > * ,unsigned int ,class std::vector<int,std::allocator<int> > & );// Offset=0x0 Size=0x2b
        void _Xlen();// Offset=0x0 Size=0x7b
        void _Xran();
        class std::vector<int,std::allocator<int> > * _Myfirst;// Offset=0x4 Size=0x4
        class std::vector<int,std::allocator<int> > * _Mylast;// Offset=0x8 Size=0x4
        class std::vector<int,std::allocator<int> > * _Myend;// Offset=0xc Size=0x4
        void * __vecDelDtor(unsigned int );
    };
};

class vec2 : public vec2_s// Size=0x8 (Id=2055)
{
    union // Size=0x4e (Id=0)
    {
        void vec2(class vec2 & );// Offset=0x0 Size=0x13
        void vec2(float ,float );// Offset=0x0 Size=0x12
        void vec2();// Offset=0x0 Size=0x3
        float length_f32();// Offset=0x0 Size=0x16
        float lengthSquared_f32();// Offset=0x0 Size=0x14
        class vec2 & normalize();// Offset=0x0 Size=0x4e
        class vec2 & operator+=(class vec2 & );// Offset=0x0 Size=0x18
        class vec2 & operator-=(class vec2 & );// Offset=0x0 Size=0x18
        class vec2 & operator*=(float );// Offset=0x0 Size=0x17
        class vec2 & operator/=(float );// Offset=0x0 Size=0x1b
    };
};

class quat// Size=0x1c (Id=2056)
{
    union // Size=0x2c (Id=0)
    {
        float w;// Offset=0x0 Size=0x4
        float x;// Offset=0x4 Size=0x4
        float y;// Offset=0x8 Size=0x4
        float z;// Offset=0xc Size=0x4
        class vec3 scale;// Offset=0x10 Size=0xc
        float length_f32();// Offset=0x0 Size=0x2c
        void quat(class quat & );
        void quat();// Offset=0x0 Size=0xf
    };
};

struct XGVECTOR3 : public _D3DVECTOR// Size=0xc (Id=2716)
{
    void XGVECTOR3(float ,float ,float );
    void XGVECTOR3(struct _D3DVECTOR & );
    void XGVECTOR3(float * );
    void XGVECTOR3();
    float * operator float *();
    float * operator const float *();
    struct XGVECTOR3 & operator+=(struct XGVECTOR3 & );
    struct XGVECTOR3 & operator-=(struct XGVECTOR3 & );
    struct XGVECTOR3 & operator*=(float );
    struct XGVECTOR3 & operator/=(float );
    struct XGVECTOR3 operator+(struct XGVECTOR3 & );
    struct XGVECTOR3 operator+();
    struct XGVECTOR3 operator-(struct XGVECTOR3 & );
    struct XGVECTOR3 operator-();
    struct XGVECTOR3 operator*(float );
    struct XGVECTOR3 operator/(float );
    int operator==(struct XGVECTOR3 & );
    int operator!=(struct XGVECTOR3 & );
};

struct XGVECTOR3 : public _D3DVECTOR// Size=0xc (Id=2717)
{
    void XGVECTOR3(float ,float ,float );
    void XGVECTOR3(struct _D3DVECTOR & );
    void XGVECTOR3(float * );
    void XGVECTOR3();
    float * operator float *();
    float * operator const float *();
    struct XGVECTOR3 & operator+=(struct XGVECTOR3 & );
    struct XGVECTOR3 & operator-=(struct XGVECTOR3 & );
    struct XGVECTOR3 & operator*=(float );
    struct XGVECTOR3 & operator/=(float );
    struct XGVECTOR3 operator+(struct XGVECTOR3 & );
    struct XGVECTOR3 operator+();
    struct XGVECTOR3 operator-(struct XGVECTOR3 & );
    struct XGVECTOR3 operator-();
    struct XGVECTOR3 operator*(float );
    struct XGVECTOR3 operator/(float );
    int operator==(struct XGVECTOR3 & );
    int operator!=(struct XGVECTOR3 & );
};

struct XGMATRIX : public _D3DMATRIX// Size=0x40 (Id=2718)
{
    void XGMATRIX(float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float );
    void XGMATRIX(struct _D3DMATRIX & );
    void XGMATRIX(float * );
    void XGMATRIX();
    float operator()(unsigned int ,unsigned int );
    float & operator()(unsigned int ,unsigned int );
    float * operator float *();
    float * operator const float *();
    struct XGMATRIX & operator*=(float );
    struct XGMATRIX & operator*=(struct XGMATRIX & );
    struct XGMATRIX & operator+=(struct XGMATRIX & );
    struct XGMATRIX & operator-=(struct XGMATRIX & );
    struct XGMATRIX & operator/=(float );
    struct XGMATRIX operator+(struct XGMATRIX & );
    struct XGMATRIX operator+();
    struct XGMATRIX operator-(struct XGMATRIX & );
    struct XGMATRIX operator-();
    struct XGMATRIX operator*(float );
    struct XGMATRIX operator*(struct XGMATRIX & );
    struct XGMATRIX operator/(float );
    int operator==(struct XGMATRIX & );
    int operator!=(struct XGMATRIX & );
};

struct XGMATRIX : public _D3DMATRIX// Size=0x40 (Id=2719)
{
    void XGMATRIX(float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float );
    void XGMATRIX(struct _D3DMATRIX & );
    void XGMATRIX(float * );
    void XGMATRIX();
    float operator()(unsigned int ,unsigned int );
    float & operator()(unsigned int ,unsigned int );
    float * operator float *();
    float * operator const float *();
    struct XGMATRIX & operator*=(float );
    struct XGMATRIX & operator*=(struct XGMATRIX & );
    struct XGMATRIX & operator+=(struct XGMATRIX & );
    struct XGMATRIX & operator-=(struct XGMATRIX & );
    struct XGMATRIX & operator/=(float );
    struct XGMATRIX operator+(struct XGMATRIX & );
    struct XGMATRIX operator+();
    struct XGMATRIX operator-(struct XGMATRIX & );
    struct XGMATRIX operator-();
    struct XGMATRIX operator*(float );
    struct XGMATRIX operator*(struct XGMATRIX & );
    struct XGMATRIX operator/(float );
    int operator==(struct XGMATRIX & );
    int operator!=(struct XGMATRIX & );
};

struct XGQUATERNION// Size=0x10 (Id=2720)
{
    void XGQUATERNION(float ,float ,float ,float );
    void XGQUATERNION(float * );
    void XGQUATERNION();
    float * operator float *();
    float * operator const float *();
    struct XGQUATERNION & operator+=(struct XGQUATERNION & );
    struct XGQUATERNION & operator-=(struct XGQUATERNION & );
    struct XGQUATERNION & operator*=(float );
    struct XGQUATERNION & operator*=(struct XGQUATERNION & );
    struct XGQUATERNION & operator/=(float );
    struct XGQUATERNION operator+(struct XGQUATERNION & );
    struct XGQUATERNION operator+();
    struct XGQUATERNION operator-(struct XGQUATERNION & );
    struct XGQUATERNION operator-();
    struct XGQUATERNION operator*(float );
    struct XGQUATERNION operator*(struct XGQUATERNION & );
    struct XGQUATERNION operator/(float );
    int operator==(struct XGQUATERNION & );
    int operator!=(struct XGQUATERNION & );
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct XGQUATERNION// Size=0x10 (Id=2721)
{
    void XGQUATERNION(float ,float ,float ,float );
    void XGQUATERNION(float * );
    void XGQUATERNION();
    float * operator float *();
    float * operator const float *();
    struct XGQUATERNION & operator+=(struct XGQUATERNION & );
    struct XGQUATERNION & operator-=(struct XGQUATERNION & );
    struct XGQUATERNION & operator*=(float );
    struct XGQUATERNION & operator*=(struct XGQUATERNION & );
    struct XGQUATERNION & operator/=(float );
    struct XGQUATERNION operator+(struct XGQUATERNION & );
    struct XGQUATERNION operator+();
    struct XGQUATERNION operator-(struct XGQUATERNION & );
    struct XGQUATERNION operator-();
    struct XGQUATERNION operator*(float );
    struct XGQUATERNION operator*(struct XGQUATERNION & );
    struct XGQUATERNION operator/(float );
    int operator==(struct XGQUATERNION & );
    int operator!=(struct XGQUATERNION & );
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct XGVECTOR4// Size=0x10 (Id=2722)
{
    void XGVECTOR4(float ,float ,float ,float );
    void XGVECTOR4(float * );
    union // Size=0x3 (Id=0)
    {
        void XGVECTOR4();// Offset=0x0 Size=0x3
        float * operator float *();
        float * operator const float *();
        struct XGVECTOR4 & operator+=(struct XGVECTOR4 & );
        struct XGVECTOR4 & operator-=(struct XGVECTOR4 & );
        struct XGVECTOR4 & operator*=(float );
        struct XGVECTOR4 & operator/=(float );
        struct XGVECTOR4 operator+(struct XGVECTOR4 & );
        struct XGVECTOR4 operator+();
        struct XGVECTOR4 operator-(struct XGVECTOR4 & );
        struct XGVECTOR4 operator-();
        struct XGVECTOR4 operator*(float );
        struct XGVECTOR4 operator/(float );
        int operator==(struct XGVECTOR4 & );
        int operator!=(struct XGVECTOR4 & );
        float x;// Offset=0x0 Size=0x4
    };
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct XGVECTOR4// Size=0x10 (Id=2723)
{
    void XGVECTOR4(float ,float ,float ,float );
    void XGVECTOR4(float * );
    union // Size=0x3 (Id=0)
    {
        void XGVECTOR4();// Offset=0x0 Size=0x3
        float * operator float *();
        float * operator const float *();
        struct XGVECTOR4 & operator+=(struct XGVECTOR4 & );
        struct XGVECTOR4 & operator-=(struct XGVECTOR4 & );
        struct XGVECTOR4 & operator*=(float );
        struct XGVECTOR4 & operator/=(float );
        struct XGVECTOR4 operator+(struct XGVECTOR4 & );
        struct XGVECTOR4 operator+();
        struct XGVECTOR4 operator-(struct XGVECTOR4 & );
        struct XGVECTOR4 operator-();
        struct XGVECTOR4 operator*(float );
        struct XGVECTOR4 operator/(float );
        int operator==(struct XGVECTOR4 & );
        int operator!=(struct XGVECTOR4 & );
        float x;// Offset=0x0 Size=0x4
    };
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct XGVECTOR2// Size=0x8 (Id=2728)
{
    void XGVECTOR2(float ,float );
    void XGVECTOR2(float * );
    void XGVECTOR2();
    float * operator float *();
    float * operator const float *();
    struct XGVECTOR2 & operator+=(struct XGVECTOR2 & );
    struct XGVECTOR2 & operator-=(struct XGVECTOR2 & );
    struct XGVECTOR2 & operator*=(float );
    struct XGVECTOR2 & operator/=(float );
    struct XGVECTOR2 operator+(struct XGVECTOR2 & );
    struct XGVECTOR2 operator+();
    struct XGVECTOR2 operator-(struct XGVECTOR2 & );
    struct XGVECTOR2 operator-();
    struct XGVECTOR2 operator*(float );
    struct XGVECTOR2 operator/(float );
    int operator==(struct XGVECTOR2 & );
    int operator!=(struct XGVECTOR2 & );
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
};

struct XGVECTOR2// Size=0x8 (Id=2729)
{
    void XGVECTOR2(float ,float );
    void XGVECTOR2(float * );
    void XGVECTOR2();
    float * operator float *();
    float * operator const float *();
    struct XGVECTOR2 & operator+=(struct XGVECTOR2 & );
    struct XGVECTOR2 & operator-=(struct XGVECTOR2 & );
    struct XGVECTOR2 & operator*=(float );
    struct XGVECTOR2 & operator/=(float );
    struct XGVECTOR2 operator+(struct XGVECTOR2 & );
    struct XGVECTOR2 operator+();
    struct XGVECTOR2 operator-(struct XGVECTOR2 & );
    struct XGVECTOR2 operator-();
    struct XGVECTOR2 operator*(float );
    struct XGVECTOR2 operator/(float );
    int operator==(struct XGVECTOR2 & );
    int operator!=(struct XGVECTOR2 & );
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
};

class quat// Size=0x1c (Id=3173)
{
    union // Size=0x2c (Id=0)
    {
        float w;// Offset=0x0 Size=0x4
        float x;// Offset=0x4 Size=0x4
        float y;// Offset=0x8 Size=0x4
        float z;// Offset=0xc Size=0x4
        class vec3 scale;// Offset=0x10 Size=0xc
        float length_f32();// Offset=0x0 Size=0x2c
        void quat(class quat & );
        void quat();// Offset=0x0 Size=0xf
    };
};

class std::_Vector_val<int,std::allocator<int> >// Size=0x1 (Id=3579)
{
    protected void _Vector_val<int,std::allocator<int> >(class std::_Vector_val<int,std::allocator<int> > & );
    union // Size=0x5 (Id=0)
    {
        void _Vector_val<int,std::allocator<int> >(class std::allocator<int> );// Offset=0x0 Size=0x5
        class std::allocator<int> _Alval;// Offset=0x0 Size=0x1
        void __dflt_ctor_closure();
    };
};

class std::_Vector_val<int,std::allocator<int> >// Size=0x1 (Id=3580)
{
    protected void _Vector_val<int,std::allocator<int> >(class std::_Vector_val<int,std::allocator<int> > & );
    union // Size=0x5 (Id=0)
    {
        void _Vector_val<int,std::allocator<int> >(class std::allocator<int> );// Offset=0x0 Size=0x5
        class std::allocator<int> _Alval;// Offset=0x0 Size=0x1
        void __dflt_ctor_closure();
    };
};

class std::_Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >// Size=0x1 (Id=3581)
{
    protected void _Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(class std::_Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
    union // Size=0x5 (Id=0)
    {
        void _Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(class std::allocator<std::vector<int,std::allocator<int> > > );// Offset=0x0 Size=0x5
        class std::allocator<std::vector<int,std::allocator<int> > > _Alval;// Offset=0x0 Size=0x1
        void __dflt_ctor_closure();
    };
};

class std::_Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >// Size=0x1 (Id=3582)
{
    protected void _Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(class std::_Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > > & );
    union // Size=0x5 (Id=0)
    {
        void _Vector_val<std::vector<int,std::allocator<int> >,std::allocator<std::vector<int,std::allocator<int> > > >(class std::allocator<std::vector<int,std::allocator<int> > > );// Offset=0x0 Size=0x5
        class std::allocator<std::vector<int,std::allocator<int> > > _Alval;// Offset=0x0 Size=0x1
        void __dflt_ctor_closure();
    };
};

struct std::allocator<std::vector<int,std::allocator<int> > >::rebind<std::vector<int,std::allocator<int> > >// Size=0x1 (Id=3583)
{
};

struct std::_Ranit<std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &> : public std::iterator<std::random_access_iterator_tag,std::vector<int,std::allocator<int> >,int,std::vector<int,std::allocator<int> > *,std::vector<int,std::allocator<int> > &>// Size=0x1 (Id=3586)
{
};


#endif // _MATH_H_
