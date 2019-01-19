#include <iostream>
#include <EEL.h>

using std::cout;
using std::endl;

class MyFancyClass
{
protected:
	int counter = 0;

public:
	MyFancyClass()
	{
		cout << "Hello from constructor" << endl;

		D_moduledata *md = (D_moduledata *)eel_malloc(vm, sizeof(D_moduledata));
		md->xxx = 4;
		EEL_object *m = eel_create_module(vm, "dir", d_unload, md);

		EEL_object *c = eel_export_class(m, "directory", -1, MyFancyClass::eel_class_constructor, NULL, NULL);
		eel_set_metamethod(c, EEL_MM_GETINDEX, d_getindex);
	}

	void SayHello()
	{
		cout << "Hello world #" << this->counter << endl;
		this->counter++;

	}

	static EEL_xno eel_class_constructor(EEL_vm *vm, EEL_types type, EEL_value *initv, int initc, EEL_value *result)
	{
		return EEL_XOK;
	}
};

int main(void)
{
	MyFancyClass *a = new MyFancyClass();
	std::cout << "Address to x" << a << std::endl;

	return 0;
}