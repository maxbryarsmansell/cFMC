#include "Utils.hpp"

#include <sstream>

#include "Utils.hpp"

std::optional<Loc_t> getLocFromId(const std::string_view &id)
{
	if      (id == k_DefaultLocId) { return k_DefaultLoc; }
	else if (id == k_NewLocId)     { return k_NewLoc; }
	else if (id == k_InputLocId)   { return k_InputLoc; }
	else if (id == k_OutputLocId)  { return k_OutputLoc; }

	return std::nullopt;
}

std::optional<std::string> getIdFromLoc(const Loc_t &loc)
{
	if      (loc == k_DefaultLoc) { return std::string(k_DefaultLocId); }
	else if (loc == k_NewLoc)     { return std::string(k_NewLocId); }
	else if (loc == k_InputLoc)   { return std::string(k_InputLocId); }
	else if (loc == k_OutputLoc)  { return std::string(k_OutputLocId); }

	return std::nullopt;
}

std::string stringifyTerm(const Term &term, bool omitNil)
{
	std::stringstream termSs;
	const Term *termPtr = nullptr;

	switch (term.kind())
	{
	case Term::Nil:
	{
		if (!omitNil)
		{
			termSs << "*";
		}
		termPtr = nullptr;
		break;
	}
	case Term::VarCont:
	{
		const VarContTerm &varCont = term.asVarCont();
		termSs << varCont.var;
		termPtr = varCont.body.get();
		break;
	}
	case Term::Abs:
	{
		const AbsTerm &abs = term.asAbs();
		if (abs.loc != k_DefaultLocId)
		{
			termSs << abs.loc;
		}
		termSs << "<" << abs.var << ">";
		termPtr = abs.body.get();
		break;
	}
	case Term::App:
	{
		const AppTerm &app = term.asApp();
		if (app.loc != k_DefaultLocId)
		{
			termSs << app.loc;
		}
		termSs << "[" << stringifyTerm(*app.arg) << "]";
		termPtr = app.body.get();
		break;
	}
	case Term::Val:
	{
		const ValTerm &val = term.asVal();
		termSs << static_cast<uint32_t>(val.val);
		termPtr = nullptr;
		break;
	}
	case Term::Cases:
	{
		const CasesTerm &cases = term.asCases();
		termSs << "(";
		for (auto itCases = cases.cases.begin(); itCases != cases.cases.end(); ++itCases)
		{
			if (itCases->first == static_cast<Val_t>(-1))
			{
				termSs << "otherwise";
			}
			else
			{
				termSs << static_cast<uint32_t>(itCases->first);
			}
			termSs  << " -> " << stringifyTerm(*itCases->second);

			auto itCasesCopy = itCases;
			if (!(++itCasesCopy == cases.cases.end()))
			{
				termSs << ", ";
			}
		}
		termSs << ")";
		break;
	}
	}

	while (termPtr)
	{
		switch (termPtr->kind())
		{
		case Term::Nil:
		{
			if (!omitNil)
			{
				termSs << " . *";
			}
			termPtr = nullptr;
			break;
		}
		case Term::VarCont:
		{
			const VarContTerm &varCont = termPtr->asVarCont();
			termSs << " . " << varCont.var;
			termPtr = varCont.body.get();
			break;
		}
		case Term::Abs:
		{
			const AbsTerm &abs = termPtr->asAbs();
			termSs << " . ";
			if (abs.loc != k_DefaultLocId)
			{
				termSs << abs.loc;
			}
			termSs << "<" << abs.var << ">";
			termPtr = abs.body.get();
			break;
		}
		case Term::App:
		{
			const AppTerm &app = termPtr->asApp();
			termSs << " . ";
			if (app.loc != k_DefaultLocId)
			{
				termSs << app.loc;
			}
			termSs << "[" << stringifyTerm(*app.arg) << "]";
			termPtr = app.body.get();
			break;
		}
		case Term::Val:
		{
			const ValTerm &val = term.asVal();
			termSs << " . " << val.val;
			termPtr = nullptr;
			break;
		}
		case Term::Cases:
		{
			const CasesTerm &cases = term.asCases();
			termSs << ". (";
			for (auto itCases = cases.cases.begin(); itCases != cases.cases.end(); ++itCases)
			{
				if (itCases->first == static_cast<Val_t>(-1))
				{
					termSs << "otherwise";
				}
				else
				{
					termSs << static_cast<uint32_t>(itCases->first);
				}
				termSs  << " -> " << stringifyTerm(*itCases->second);

				auto itCasesCopy = itCases;
				if (!(++itCasesCopy == cases.cases.end()))
				{
					termSs << ", ";
				}
			}
			termSs << ")";
			break;
		}
		}
	}

	return termSs.str();
}