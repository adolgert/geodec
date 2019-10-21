#ifndef _GENERATE_LAND_HPP_
#define _GENERATE_LAND_HPP_ 1


namespace geodec
{



    template<class USAGE>
    class compare_land_uses
    {
        USAGE& _use;
    public:
        compare_land_uses(USAGE& use) : _use(use) {}
        bool operator()(size_t a, size_t b) {
            //std::cout << "compare_land_uses " << a << " " << b << std::endl;
            return get(_use, a)==get(_use, b);
        }
    };

}


#endif // _GENERATE_LAND_HPP_
