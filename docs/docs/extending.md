---
permalink: /docs/extending
base: /docs
---

Extending EEL
=============


Important notes about EEL Objects
---------------------------------
Normally, you should NEVER INSTANTIATE class "body" structs,
like EEL_vector, EEL_table, EEL_function, EEL_module or
corresponding structs for user defined classes. Doing so
WILL NOT RESULT IN VALID EEL OBJECTS.

* Use EEL_ALLOC_OBJECT() to create proper EEL object with
  headers.
* Use EEL_MAKE_CAST(MyType) to create an inline function
  MyType `*o2MyType(EEL_object *o)`, that casts an EEL_object
  pointer to a pointer to MyType.



Adding a C function
-------------------

1. Implement the function:

	```c
	static int my_cfunction(EEL_vm *vm)
	{
		EEL_value *arg = vm->heap + vm->argv;
		EEL_value *res = vm->heap + vm->resv;
		if(arg[0].type != EEL_TINTEGER)
			return EEL_XWRONGTYPE;
		if(arg[1].type != EEL_TINTEGER)
			return EEL_XWRONGTYPE;
		res->type = EEL_TINTEGER;
		res->integer.v = arg[0].integer.v + arg[1].integer.v;
	}
	```

2. Register the function with the compiler:

	```c
	eel_register_cfunction(es, 1, "sum", 2, 0, 0, my_function);
	```

	Where the arguments are state, number of results, name in EEL,
	number of required arguments, number of optional arguments,
	number of tuple arguments and the address of the function.



Adding a class
--------------

1. Define a "body struct" to hold the private data of
  an instance:

	```c
	typedef struct
	{
		int	some_field;
		int	some_other_field;
	} MY_class;
	```

2. Define a cast function to get at your struct inside
  an instance:

	```c
	EEL_MAKE_CAST(MY_class)
	```

3. Implement a constructor:

	```c
	static EEL_exceptions mc_construct(EEL_vm *vm, EEL_object *eo,
			EEL_value *op1, EEL_value *op2)
	{
		MY_class *mc;
		eo = EEL_ALLOC_OBJECT(vm, MY_class);
		if(!eo)
			return EEL_XMEMORY;
		mc = o2MY_class(eo);
		mc->some_field = ...;
		...
		op1->type = EEL_TOBJECT;
		op1->object.v = eo;
		return 0;
	}
	```

4. Implement a destructor:

	```c
	static EEL_exceptions mc_destruct(EEL_vm *vm, EEL_object *eo,
			EEL_value *op1, EEL_value *op2)
	{
		eel_free(vm, eo);
		return 0;
	}
	```

5. Register the class with the compiler:

	```c
	int my_class_cid = eel_add_class(es, "myclass", mc_create, mc_destroy);
	```

6. Implement some metamethods (one in our example):

	```c
	static EEL_exceptions mc_setindex(EEL_vm *vm, EEL_object *eo,
			EEL_value *op1, EEL_value *op2)
	{
		MY_class *mc = o2MY_class(eo);
		if(op1->type != EEL_TINTEGER)
			return EEL_XWRONGTYPE;
		if(op2->type != EEL_TINTEGER)
			return EEL_XWRONGTYPE;
		switch(op2->integer.v)
		{
		  case 0:
			mc->some_field = op1->integer.v;
			break;
		  case 1:
			mc->some_other_field = op1->integer.v;
			break;
		  default:
			return EEL_XWRONGINDEX;
		}
		op1->type = EEL_TINTEGER;
		return 0;
	}
	```

7. Register your metamethods with your class:

	```c
	eel_set_metamethod(es, my_class_cid, EEL_MM_SETINDEX, mc_setindex);
	```