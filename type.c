#include "rvcc.h"

// (Type){...}构造了一个复合字面量，相当于Type的匿名变量。
Type *TyVoid = &(Type){TY_VOID, 1, 1};
Type *TyBool = &(Type){TY_BOOL, 1, 1};

Type *TyChar = &(Type){TY_CHAR, 1, 1};
Type *TyShort = &(Type){TY_SHORT, 2, 2};
Type *TyInt = &(Type){TY_INT, 4, 4};
Type *TyLong = &(Type){TY_LONG, 8, 8};

Type *TyUChar = &(Type){TY_CHAR, 1, 1, true};
Type *TyUShort = &(Type){TY_SHORT, 2, 2, true};
Type *TyUInt = &(Type){TY_INT, 4, 4, true};
Type *TyULong = &(Type){TY_LONG, 8, 8, true};

Type *TyFloat = &(Type){TY_FLOAT, 4, 4};
Type *TyDouble = &(Type){TY_DOUBLE, 8, 8};
Type *TyLDouble = &(Type){TY_LDOUBLE, 16, 16};

static Type *newType(TypeKind Kind, int Size, int Align) {
  Type *Ty = calloc(1, sizeof(Type));
  Ty->Kind = Kind;
  Ty->Size = Size;
  Ty->Align = Align;
  return Ty;
}

// 判断Type是否为整数
bool isInteger(Type *Ty) {
  TypeKind K = Ty->Kind;
  return K == TY_BOOL || K == TY_CHAR || K == TY_SHORT || K == TY_INT ||
         K == TY_LONG || K == TY_ENUM;
}

// 判断Type是否为浮点数
bool isFloNum(Type *Ty) {
  return Ty->Kind == TY_FLOAT || Ty->Kind == TY_DOUBLE ||
         Ty->Kind == TY_LDOUBLE;
}

// 判断是否为float或double，而非long double
bool isFloatOrDouble(Type *Ty) {
  return Ty->Kind == TY_FLOAT || Ty->Kind == TY_DOUBLE;
}

// 判断是否为数字
bool isNumeric(Type *Ty) { return isInteger(Ty) || isFloNum(Ty); }

bool isCompatible(Type *T1, Type *T2) {
  if (T1 == T2)
    return true;

  if (T1->Origin)
    return isCompatible(T1->Origin, T2);

  if (T2->Origin)
    return isCompatible(T1, T2->Origin);

  if (T1->Kind != T2->Kind)
    return false;

  switch (T1->Kind) {
  case TY_CHAR:
  case TY_SHORT:
  case TY_INT:
  case TY_LONG:
    return T1->IsUnsigned == T2->IsUnsigned;
  case TY_FLOAT:
  case TY_DOUBLE:
  case TY_LDOUBLE:
    return true;
  case TY_PTR:
    return isCompatible(T1->Base, T2->Base);
  case TY_FUNC: {
    if (!isCompatible(T1->ReturnTy, T2->ReturnTy))
      return false;
    if (T1->IsVariadic != T2->IsVariadic)
      return false;

    Type *p1 = T1->Params;
    Type *p2 = T2->Params;
    for (; p1 && p2; p1 = p1->Next, p2 = p2->Next)
      if (!isCompatible(p1, p2))
        return false;
    return p1 == NULL && p2 == NULL;
  }
  case TY_ARRAY:
    if (!isCompatible(T1->Base, T2->Base))
      return false;
    return T1->ArrayLen < 0 && T2->ArrayLen < 0 && T1->ArrayLen == T2->ArrayLen;
  default:
    return false;
  }
}

// 复制类型
Type *copyType(Type *Ty) {
  Type *Ret = calloc(1, sizeof(Type));
  *Ret = *Ty;
  Ret->Origin = Ty;
  return Ret;
}

// 指针类型，并且指向基类
Type *pointerTo(Type *Base) {
  Type *Ty = newType(TY_PTR, 8, 8);
  Ty->Base = Base;
  // 将指针作为无符号类型进行比较
  Ty->IsUnsigned = true;
  return Ty;
}

// 函数类型，并赋返回类型
Type *funcType(Type *ReturnTy) {
  // The C spec disallows sizeof(<function type>), but
  // GCC allows that and the expression is evaluated to 1.
  Type *Ty = newType(TY_FUNC, 1, 1);
  Ty->ReturnTy = ReturnTy;
  return Ty;
}

// 构造数组类型, 传入 数组基类, 元素个数
Type *arrayOf(Type *Base, int Len) {
  Type *Ty = newType(TY_ARRAY, Base->Size * Len, Base->Align);
  Ty->Base = Base;
  Ty->ArrayLen = Len;
  return Ty;
}

// 构造可变长数组类型
Type *VLAOf(Type *Base, Node *Len) {
  Type *Ty = newType(TY_VLA, 8, 8);
  Ty->Base = Base;
  Ty->VLALen = Len;
  return Ty;
}

// 构造枚举类型
Type *enumType(void) { return newType(TY_ENUM, 4, 4); }

// 构造结构体类型
Type *structType(void) { return newType(TY_STRUCT, 0, 1); }

// 获取容纳左右部的类型
static Type *getCommonType(Type *Ty1, Type *Ty2) {
  if (Ty1->Base)
    return pointerTo(Ty1->Base);

  // 为函数指针进行常规算术转换
  if (Ty1->Kind == TY_FUNC)
    return pointerTo(Ty1);
  if (Ty2->Kind == TY_FUNC)
    return pointerTo(Ty2);

  // 处理浮点类型
  // 优先使用long double类型
  if (Ty1->Kind == TY_LDOUBLE || Ty2->Kind == TY_LDOUBLE)
    return TyLDouble;
  // 其次使用double类型
  if (Ty1->Kind == TY_DOUBLE || Ty2->Kind == TY_DOUBLE)
    return TyDouble;
  // 其次使用float类型
  if (Ty1->Kind == TY_FLOAT || Ty2->Kind == TY_FLOAT)
    return TyFloat;

  // 小于四字节则为int
  if (Ty1->Size < 4)
    Ty1 = TyInt;
  if (Ty2->Size < 4)
    Ty2 = TyInt;

  // 选择二者中更大的类型
  if (Ty1->Size != Ty2->Size)
    return (Ty1->Size < Ty2->Size) ? Ty2 : Ty1;

  // 优先返回无符号类型（更大）
  if (Ty2->IsUnsigned)
    return Ty2;
  return Ty1;
}

// 进行常规的算术转换
static void usualArithConv(Node **LHS, Node **RHS) {
  Type *Ty = getCommonType((*LHS)->Ty, (*RHS)->Ty);
  // 将左右部转换到兼容的类型
  *LHS = newCast(*LHS, Ty);
  *RHS = newCast(*RHS, Ty);
}

// 为节点内的所有节点添加类型
void addType(Node *Nd) {
  // 判断 节点是否为空 或者 节点类型已经有值，那么就直接返回
  if (!Nd || Nd->Ty)
    return;

  // 递归访问所有节点以增加类型
  addType(Nd->LHS);
  addType(Nd->RHS);
  addType(Nd->Cond);
  addType(Nd->Then);
  addType(Nd->Els);
  addType(Nd->Init);
  addType(Nd->Inc);

  // 访问链表内的所有节点以增加类型
  for (Node *N = Nd->Body; N; N = N->Next)
    addType(N);
  // 访问链表内的所有参数节点以增加类型
  for (Node *N = Nd->Args; N; N = N->Next)
    addType(N);

  switch (Nd->Kind) {
  // 将节点类型设为 int
  case ND_NUM:
    Nd->Ty = TyInt;
    return;
  // 将节点类型设为 节点左部的类型
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
    // 对左右部转换
    usualArithConv(&Nd->LHS, &Nd->RHS);
    Nd->Ty = Nd->LHS->Ty;
    return;
  case ND_NEG: {
    // 对左部转换
    Type *Ty = getCommonType(TyInt, Nd->LHS->Ty);
    Nd->LHS = newCast(Nd->LHS, Ty);
    Nd->Ty = Ty;
    return;
  }
    // 将节点类型设为 节点左部的类型
    // 左部不能是数组节点
  case ND_ASSIGN:
    if (Nd->LHS->Ty->Kind == TY_ARRAY)
      errorTok(Nd->LHS->Tok, "not an lvalue");
    if (Nd->LHS->Ty->Kind != TY_STRUCT)
      // 对右部转换
      Nd->RHS = newCast(Nd->RHS, Nd->LHS->Ty);
    Nd->Ty = Nd->LHS->Ty;
    return;
  // 将节点类型设为 int
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    // 对左右部转换
    usualArithConv(&Nd->LHS, &Nd->RHS);
    Nd->Ty = TyInt;
    return;
  case ND_FUNCALL:
    Nd->Ty = Nd->FuncType->ReturnTy;
    return;
  // 将节点类型设为 int
  case ND_NOT:
  case ND_LOGOR:
  case ND_LOGAND:
    Nd->Ty = TyInt;
    return;
  // 将节点类型设为 左部的类型
  case ND_BITNOT:
  case ND_SHL:
  case ND_SHR:
    Nd->Ty = Nd->LHS->Ty;
    return;
  // 将节点类型设为 变量的类型
  case ND_VAR:
  case ND_VLA_PTR:
    Nd->Ty = Nd->Var->Ty;
    return;
  // 如果:左或右部为void则为void，否则为二者兼容的类型
  case ND_COND:
    if (Nd->Then->Ty->Kind == TY_VOID || Nd->Els->Ty->Kind == TY_VOID) {
      Nd->Ty = TyVoid;
    } else {
      usualArithConv(&Nd->Then, &Nd->Els);
      Nd->Ty = Nd->Then->Ty;
    }
    return;
  // 将节点类型设为 右部的类型
  case ND_COMMA:
    Nd->Ty = Nd->RHS->Ty;
    return;
  // 将节点类型设为 成员的类型
  case ND_MEMBER:
    Nd->Ty = Nd->Mem->Ty;
    return;
  // 将节点类型设为 指针，并指向左部的类型
  case ND_ADDR: {
    Type *Ty = Nd->LHS->Ty;
    // 左部如果是数组, 则为指向数组基类的指针
    if (Ty->Kind == TY_ARRAY)
      Nd->Ty = pointerTo(Ty->Base);
    else
      Nd->Ty = pointerTo(Ty);
    return;
  }
  // 节点类型：如果解引用指向的是指针，则为指针指向的类型；否则报错
  case ND_DEREF:
    // 如果不存在基类, 则无法解引用
    if (!Nd->LHS->Ty->Base)
      errorTok(Nd->Tok, "invalid pointer dereference");
    if (Nd->LHS->Ty->Base->Kind == TY_VOID)
      errorTok(Nd->Tok, "dereferencing a void pointer");

    Nd->Ty = Nd->LHS->Ty->Base;
    return;
  // 节点类型为 最后的表达式语句的类型
  case ND_STMT_EXPR:
    if (Nd->Body) {
      Node *Stmt = Nd->Body;
      while (Stmt->Next)
        Stmt = Stmt->Next;
      if (Stmt->Kind == ND_EXPR_STMT) {
        Nd->Ty = Stmt->LHS->Ty;
        return;
      }
    }
    errorTok(Nd->Tok, "statement expression returning void is not supported");
    return;
  case ND_LABEL_VAL:
    Nd->Ty = pointerTo(TyVoid);
    return;
  default:
    break;
  }
}
