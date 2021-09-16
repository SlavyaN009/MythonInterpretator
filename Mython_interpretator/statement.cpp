#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = rv_.get()->Execute(closure, context);
    return closure.at(var_);
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
    : var_(move(var))
    , rv_(move(rv)) {}

VariableValue::VariableValue(const std::string& var_name) 
    : dotted_ids_({ var_name }) {}

VariableValue::VariableValue(std::vector<std::string> dotted_ids) 
    : dotted_ids_(move(dotted_ids)) {}

ObjectHolder VariableValue::Execute(Closure& closure, Context&) {
    size_t ids_count = dotted_ids_.size();
    Closure new_closure = closure;
    for (size_t i = 0; i < ids_count; ++i) {
        if (new_closure.count(dotted_ids_[i])) {
            if (i == ids_count - 1) return new_closure.at(dotted_ids_[i]);
            if (auto* base_ci = new_closure.at(dotted_ids_[i]).TryAs<runtime::ClassInstance>()) {
                new_closure = base_ci->Fields();
            }
            else {
                throw runtime_error("Cannot cast to ClassInstance"s);
            }
        }
        else {
            throw runtime_error("No such variables"s);
        }
    }
    throw runtime_error("No such variables at all"s);
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return make_unique<Print>(make_unique<VariableValue>(name));
}

Print::Print(unique_ptr<Statement> argument)
{
    args_.push_back(move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args)
    : args_(move(args)){}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    ostringstream os;
    for (size_t i = 0; i < args_.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        ObjectHolder oh = args_[i].get()->Execute(closure, context);
        if (runtime::Object* obj = oh.Get()) {
            obj->Print(os, context);
        }
        else {
            os << "None"s;
        }
    }
    context.GetOutputStream() << os.str() << endl;
    runtime::String str_obj(os.str());
    return ObjectHolder::Own(move(str_obj));
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args) 
    : object_(move(object))
    , method_(move(method))
    , args_(move(args)) {}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    ObjectHolder oh = object_.get()->Execute(closure, context);
    if (auto* ci = oh.TryAs<runtime::ClassInstance>()) {
        vector<ObjectHolder> actual_args;
        actual_args.reserve(args_.size());
        for (const auto& arg : args_) {
            actual_args.push_back(arg.get()->Execute(closure, context));
        }
        return ci->Call(method_, actual_args, context);
    }else 
        throw runtime_error("Not a ClassInstance"s);
}

UnaryOperation::UnaryOperation(std::unique_ptr<Statement> argument) 
    : argument_(move(argument)){}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    ostringstream os;
    ObjectHolder oh = argument_.get()->Execute(closure, context);
    if (runtime::Object* obj = oh.Get()) {
        obj->Print(os, context);
    }
    else {
        os << "None"s;
    }
    runtime::String str_obj(os.str());
    return ObjectHolder::Own(move(str_obj));
}

BinaryOperation::BinaryOperation(std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs)
    : lhs_(move(lhs))
    , rhs_(move(rhs)) {}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_n = lhs_oh.TryAs<runtime::Number>()) {
        if (auto* rhs_n = rhs_oh.TryAs<runtime::Number>()) {
            int val = lhs_n->GetValue() + rhs_n->GetValue();
            runtime::Number result(val);
            return ObjectHolder::Own(move(result));
        }
    }
    else if (auto* lhs_s = lhs_oh.TryAs<runtime::String>()) {
        if (auto* rhs_s = rhs_oh.TryAs<runtime::String>()) {
            string str = lhs_s->GetValue() + rhs_s->GetValue();
            runtime::String result(str);
            return ObjectHolder::Own(move(result));
        }
    }
    else if (auto* lhs_ci = lhs_oh.TryAs<runtime::ClassInstance>()) {
        if (lhs_ci->HasMethod(ADD_METHOD, 1)) {
            return lhs_ci->Call(ADD_METHOD, { rhs_oh }, context);
        }
    }
    throw runtime_error("Cannot add"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_n = lhs_oh.TryAs<runtime::Number>()) {
        if (auto* rhs_n = rhs_oh.TryAs<runtime::Number>()) {
            int val = lhs_n->GetValue() - rhs_n->GetValue();
            runtime::Number result(val);
            return ObjectHolder::Own(move(result));
        }
    }
    throw runtime_error("Cannot sub"s);
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_n = lhs_oh.TryAs<runtime::Number>()) {
        if (auto* rhs_n = rhs_oh.TryAs<runtime::Number>()) {
            int val = lhs_n->GetValue() * rhs_n->GetValue();
            runtime::Number result(val);
            return ObjectHolder::Own(move(result));
        }
    }
    throw runtime_error("Cannot Mult"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_n = lhs_oh.TryAs<runtime::Number>()) {
        if (auto* rhs_n = rhs_oh.TryAs<runtime::Number>()) {
            if (int rhs_val = rhs_n->GetValue()) {
                int val = lhs_n->GetValue() / rhs_val;
                runtime::Number result(val);
                return ObjectHolder::Own(move(result));
            }
        }
    }
    throw runtime_error("Cannot Div"s);
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (const auto& st : statements_) {
        st.get()->Execute(closure, context);
    }
    return ObjectHolder::None();
}

Return::Return(std::unique_ptr<Statement> statement) 
    : statement_(move(statement)) {}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    throw statement_->Execute(closure, context);
}

ClassDefinition::ClassDefinition(ObjectHolder cls)
: cls_(move(cls)){}

ObjectHolder ClassDefinition::Execute(Closure& closure, Context&) {
    if (runtime::Class* c = cls_.TryAs<runtime::Class>()) {
        closure[c->GetName()] = cls_;
        return cls_;
    }
    throw runtime_error("Not a Class"s);
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv)
    : object_(move(object))
    , field_name_(move(field_name)) 
    , rv_(move(rv)) {
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    ObjectHolder base = object_.Execute(closure, context);
    if (auto* base_ci = base.TryAs<runtime::ClassInstance>()) {
        base_ci->Fields()[field_name_] = rv_.get()->Execute(closure, context);
        closure[field_name_] = base_ci->Fields().at(field_name_);
        return base_ci->Fields().at(field_name_);
    }
    throw runtime_error("Cannot assign field"s);
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body)
    : condition_(move(condition))
    , if_body_(move(if_body))
    , else_body_(move(else_body)) {}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    ObjectHolder condition = condition_.get()->Execute(closure, context);
    if (auto* b = condition.TryAs<runtime::Bool>()) {
        if (b->GetValue()) {
            return if_body_->Execute(closure, context);
        }
        else if (else_body_.get()) {
            return else_body_->Execute(closure, context);
        }
        else return ObjectHolder::None();
    }
    else
        throw runtime_error("No bool condition"s);
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_b = lhs_oh.TryAs<runtime::Bool>()) {
        if (!lhs_b->GetValue()) {
            if (auto* rhs_b = rhs_oh.TryAs<runtime::Bool>()) {
                if (rhs_b->GetValue()) {
                    runtime::Bool result(true);
                    return ObjectHolder::Own(move(result));
                }
                else {
                    runtime::Bool result(false);
                    return ObjectHolder::Own(move(result));
                }
            }
        }
        else {
            runtime::Bool result(true);
            return ObjectHolder::Own(move(result));
        }
    }
    throw runtime_error("Cannot compare Or"s);
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_b = lhs_oh.TryAs<runtime::Bool>()) {
        if (lhs_b->GetValue()) {
            if (auto* rhs_b = rhs_oh.TryAs<runtime::Bool>()) {
                if (rhs_b->GetValue()) {
                    runtime::Bool result(true);
                    return ObjectHolder::Own(move(result));
                }
                else {
                    runtime::Bool result(false);
                    return ObjectHolder::Own(move(result));
                }
            }
        }
        else {
            runtime::Bool result(false);
            return ObjectHolder::Own(move(result));
        }
    }
    throw runtime_error("Cannot compare And"s);
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    ObjectHolder oh = argument_.get()->Execute(closure, context);
    if (auto* b = oh.TryAs<runtime::Bool>()) {
        runtime::Bool result(!b->GetValue());
        return ObjectHolder::Own(move(result));
    }
    throw runtime_error("Cannot Not operation"s);
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs))
    , cmp_(move(cmp)){}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    runtime::Bool result(cmp_(lhs_oh, rhs_oh, context));
    return ObjectHolder::Own(move(result));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
    : ci_(class_)
    , args_(move(args)) {}

NewInstance::NewInstance(const runtime::Class& class_) 
    : ci_(class_) {}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    if (ci_.HasMethod(INIT_METHOD, args_.size())) {
        vector<ObjectHolder> actual_args;
        actual_args.reserve(args_.size());
        for (const auto& arg : args_) {
            actual_args.push_back(arg.get()->Execute(closure, context));
        }
        ci_.Call(INIT_METHOD, actual_args, context);
    }
    return ObjectHolder::Share(ci_);
    
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
    : body_(move(body)){}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        return body_->Execute(closure, context);
    }
    catch (ObjectHolder result) {
        return result;
    }
}

}  // namespace ast