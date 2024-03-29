#include "Parser.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>

#include "Utils.hpp"

static void parseError(std::string message, const Lexer &lexer)
{
	if (auto tokenBufferOpt = lexer.getPeekBuffer())
	{
		std::cerr << "[Parse Error]" << std::endl;

		auto findSub = [](const std::string& str, const std::string& sub) {
			std::size_t res = str.rfind(sub, str.size() - sub.size());
			return (res != std::string::npos) ? std::optional(res) : std::nullopt;
		};

		const std::string buffer = lexer.getBuffer();
		std::string tokenBuffer = tokenBufferOpt.value();
		size_t tokenBufferPos = 0;
		size_t tokenBufferLen = 0;
		
		std::string line;
		std::stringstream ss(buffer);
		while (std::getline(ss, line))
		{
			std::cerr << "| " << line << std::endl;

			if (auto tokenBufferPosOpt = findSub(line, tokenBuffer))
			{
				tokenBufferPos = tokenBufferPosOpt.value();
				tokenBufferLen = tokenBuffer.size();
			}
		}
		
		std::string err1;
		err1 += std::string(tokenBufferPos, ' ');
		err1 += std::string(tokenBufferLen, '^');
		err1 += " ";
		err1 += message;

		std::cerr << "| " << err1 << std::endl;
		
		std::string err2;
		err2 += std::string(tokenBufferPos, ' ');
		err2 += "| Got '";
		err2 += tokenBuffer;
		err2 += "'";

		std::cerr << "| " << err2 << std::endl;
	}

	std::exit(1);
}

Parser::Parser() : m_Lexer(nullptr) {}

Program Parser::parseProgram(const std::string &programSrc)
{
	m_Lexer = std::make_unique<Lexer>(programSrc);

	Program::FuncDefs_t funcs = parseFuncDefs();
	return Program(std::move(funcs));
}

std::optional<Term> Parser::parseTerm(const std::string &termSrc)
{
	m_Lexer = std::make_unique<Lexer>(termSrc);

	if (auto termOpt = parseTerm())
	{
		return std::move(termOpt.value());
	}

	return std::nullopt;
}

Program::FuncDefs_t Parser::parseFuncDefs()
{
	Program::FuncDefs_t funcs;
	
	while (!m_Lexer->isPeekToken(Token::Eof))
	{
		if (m_Lexer->isPeekToken(Token::Id))
		{
			if (auto funcOpt = m_Lexer->getPeekBuffer())
			{
				m_Lexer->next();

				if (m_Lexer->isPeekToken(Token::Equal))
				{
					m_Lexer->next();

					if (m_Lexer->isPeekToken(Token::Lb))
					{
						m_Lexer->next();

						if (auto termOpt = parseTerm())
						{
							if (m_Lexer->isPeekToken(Token::Rb))
							{
								m_Lexer->next();
								funcs[funcOpt.value()] = newTerm(std::move(termOpt.value()));
							}
							else
							{
								parseError("Expected ')' after definition of function '" + funcOpt.value() + "'", *m_Lexer);
							}
						}
						else
						{
							parseError("Expected term for definition of function '" + funcOpt.value() + "'", *m_Lexer);
						}
					}
					else
					{
						parseError("Expected '(' before definition of function '" + funcOpt.value() + "'", *m_Lexer);
					}
				}
				else
				{
					parseError("Expected '=' after declaration of function '" + funcOpt.value() + "'", *m_Lexer);
				}
			}
		}
		else
		{
			parseError("Expected function declaration", *m_Lexer);
		}
	}

	return funcs;
}

std::optional<Term> Parser::parseTerm()
{
	if (m_Lexer->isPeekToken(Token::Asterisk))
	{
		m_Lexer->next();
		return Term(NilTerm());
	}
	else if (auto varOpt = parseVar())
	{
		return Term(std::move(varOpt.value()));
	}
	else if(auto locAbsOpt = parseLocAbs())
	{
		return Term(std::move(locAbsOpt.value()));
	}
	else if (auto locAppOpt = parseLocApp())
	{
		return Term(std::move(locAppOpt.value()));
	}
	else if (auto absOpt = parseAbs())
	{
		return Term(std::move(absOpt.value()));
	}
	else if (auto appOpt = parseApp())
	{
		return Term(std::move(appOpt.value()));
	}
	else if (auto valOpt = parseVal())
	{
		return Term(std::move(valOpt.value()));
	}
	else if (auto binOpOpt = parseBinOp())
	{
		return Term(std::move(binOpOpt.value()));
	}
	else if (auto casesOpt = parseCases())
	{
		if (std::holds_alternative<CasesTerm<Prim_t>>(casesOpt.value()))
		{
			return Term(std::move(std::get<CasesTerm<Prim_t>>(casesOpt.value())));
		}
		else if (std::holds_alternative<CasesTerm<Loc_t>>(casesOpt.value()))
		{
			return Term(std::move(std::get<CasesTerm<Loc_t>>(casesOpt.value())));
		}
	}

	return std::nullopt;
}

std::optional<VarTerm> Parser::parseVar()
{
	if (m_Lexer->isPeekToken(Token::Id))
	{
		if (auto varOpt = m_Lexer->getPeekBuffer())
		{
			if (m_Lexer->isPeekToken(Token::Dot, 1))
			{
				m_Lexer->next();
				m_Lexer->next();
				
				if (auto bodyOpt = parseTerm())
				{
					return VarTerm(varOpt.value(), std::move(bodyOpt.value()));
				}
				else
				{
					parseError("Expected term after variable '" + varOpt.value() + "'", *m_Lexer);
				}
			}
			else if (m_Lexer->isPeekToken(Token::Comma, 1) ||
					 m_Lexer->isPeekToken(Token::Rsb, 1) ||
					 m_Lexer->isPeekToken(Token::Rb, 1) ||
					 m_Lexer->isPeekToken(Token::Eof, 1))
			{
				m_Lexer->next();
				return VarTerm(varOpt.value());
			}
		}
	}

	return std::nullopt;
}

std::optional<AbsTerm> Parser::parseAbs()
{
	std::optional<std::string> locOpt;

	if (m_Lexer->isPeekToken(Token::Id) &&
		m_Lexer->isPeekToken(Token::Lab, 1) &&
		!m_Lexer->isPeekToken(Token::Ampersand, 2))
	{
		locOpt = m_Lexer->getPeekBuffer();
		m_Lexer->next();
	}

	if (m_Lexer->isPeekToken(Token::Lab) &&
		!m_Lexer->isPeekToken(Token::Ampersand, 1))
	{
		m_Lexer->next();

		std::optional<std::string> varOpt;
		
		if (m_Lexer->isPeekToken(Token::Id))
		{
			varOpt = m_Lexer->getPeekBuffer();
			m_Lexer->next();
		}
		else if (m_Lexer->isPeekToken(Token::Underscore))
		{
			m_Lexer->next();
		}
		else
		{
			parseError("Expected binding variable of abstraction", *m_Lexer);
		}

		if (m_Lexer->isPeekToken(Token::Rab))
		{
			m_Lexer->next();

			if (m_Lexer->isPeekToken(Token::Dot))
			{
				m_Lexer->next();
				
				if (auto bodyOpt = parseTerm())
				{
					return AbsTerm(
						locOpt.value_or(k_LambdaLoc), varOpt,
						std::move(bodyOpt.value())
					);
				}
				else
				{
					parseError("Expected term after abstraction", *m_Lexer);
				}
			}

			return AbsTerm(
				locOpt.value_or(k_LambdaLoc), varOpt
			);
		}
		else
		{
			parseError("Expected closing '>' of abstraction", *m_Lexer);
		}
	}

	return std::nullopt;
}

std::optional<AppTerm> Parser::parseApp()
{
	if (m_Lexer->isPeekToken(Token::Lsb) &&
		!m_Lexer->isPeekToken(Token::Hash, 1))
	{
		m_Lexer->next();
		
		if (auto argOpt = parseTerm())
		{
			if (m_Lexer->isPeekToken(Token::Rsb))
			{
				m_Lexer->next();

				std::optional<std::string> locOpt;

				if (m_Lexer->isPeekToken(Token::Id))
				{
					locOpt = m_Lexer->getPeekBuffer();
					m_Lexer->next();
				}

				if (m_Lexer->isPeekToken(Token::Dot))
				{
					m_Lexer->next();
					
					if (auto bodyOpt = parseTerm())
					{
						return AppTerm(
							locOpt.value_or(k_LambdaLoc),
							std::move(argOpt.value()),
							std::move(bodyOpt.value())
						);
					}
					else
					{
						parseError("Expected term after application", *m_Lexer);
					}
				}

				return AppTerm(
					locOpt.value_or(k_LambdaLoc),
					std::move(argOpt.value())
				);
			}
			else
			{
				parseError("Expected closing ']' of application", *m_Lexer);
			}
		}
		else
		{
			parseError("Expected inner term of application", *m_Lexer);
		}
	}

	return std::nullopt;
}

std::optional<LocAbsTerm> Parser::parseLocAbs()
{
	std::optional<std::string> locOpt;

	if (m_Lexer->isPeekToken(Token::Id) &&
		m_Lexer->isPeekToken(Token::Lab, 1) &&
		m_Lexer->isPeekToken(Token::Ampersand, 2))
	{
		locOpt = m_Lexer->getPeekBuffer();
		m_Lexer->next();
	}

	if (m_Lexer->isPeekToken(Token::Lab) &&
		m_Lexer->isPeekToken(Token::Ampersand, 1))
	{
		m_Lexer->next();
		m_Lexer->next();

		std::optional<std::string> varOpt;
		
		if (m_Lexer->isPeekToken(Token::Id))
		{
			varOpt = m_Lexer->getPeekBuffer();
			m_Lexer->next();
		}
		else if (m_Lexer->isPeekToken(Token::Underscore))
		{
			m_Lexer->next();
		}
		else
		{
			parseError("Expected binding variable of abstraction", *m_Lexer);
		}

		if (m_Lexer->isPeekToken(Token::Rab))
		{
			m_Lexer->next();
			
			if (m_Lexer->isPeekToken(Token::Dot))
			{
				m_Lexer->next();

				if (auto bodyOpt = parseTerm())
				{
					return LocAbsTerm(
						locOpt.value_or(k_LambdaLoc), varOpt,
						std::move(bodyOpt.value())
					);
				}
				else
				{
					parseError("Expected term after abstraction", *m_Lexer);
				}
			}

			return LocAbsTerm(
				locOpt.value_or(k_LambdaLoc), varOpt
			);
		}
		else
		{
			parseError("Expected closing '>' of abstraction", *m_Lexer);
		}
	}

	return std::nullopt;
}

std::optional<LocAppTerm> Parser::parseLocApp()
{
	if (m_Lexer->isPeekToken(Token::Lsb) &&
		m_Lexer->isPeekToken(Token::Hash, 1))
	{
		m_Lexer->next();
		m_Lexer->next();
		
		if (m_Lexer->isPeekToken(Token::Id))
		{
			if (auto argOpt = m_Lexer->getPeekBuffer())
			{
				m_Lexer->next();

				if (m_Lexer->isPeekToken(Token::Rsb))
				{
					m_Lexer->next();

					std::optional<std::string> locOpt;

					if (m_Lexer->isPeekToken(Token::Id))
					{
						locOpt = m_Lexer->getPeekBuffer();
						m_Lexer->next();
					}

					if (m_Lexer->isPeekToken(Token::Dot))
					{
						m_Lexer->next();
						
						if (auto bodyOpt = parseTerm())
						{
							return LocAppTerm(
								locOpt.value_or(k_LambdaLoc),
								argOpt.value(),
								std::move(bodyOpt.value())
							);
						}
						else
						{
							parseError("Expected term after application", *m_Lexer);
						}
					}

					return LocAppTerm(
						locOpt.value_or(k_LambdaLoc),
						argOpt.value()
					);
				}
				else
				{
					parseError("Expected closing ']' of application", *m_Lexer);
				}
			}
		}
		else
		{
			parseError("Expected inner term of application", *m_Lexer);
		}
	}

	return std::nullopt;
}

std::optional<ValTerm> Parser::parseVal()
{
	if (m_Lexer->isPeekToken(Token::Primitive))
	{
		if (auto primOpt = m_Lexer->getPeekBuffer())
		{
			m_Lexer->next();

			int prim = std::stoi(primOpt.value());
			return ValTerm(
				prim
			);
		}
	}

	return std::nullopt;
}

std::optional<BinOpTerm> Parser::parseBinOp()
{
	if (m_Lexer->isPeekToken(Token::Plus) ||
		m_Lexer->isPeekToken(Token::Minus))
	{
		BinOpTerm::Op op;
		if (m_Lexer->isPeekToken(Token::Plus))
		{
			op = BinOpTerm::Plus;
		}
		else if (m_Lexer->isPeekToken(Token::Minus))
		{
			op = BinOpTerm::Minus;
		}

		m_Lexer->next();

		if (m_Lexer->isPeekToken(Token::Dot))
		{
			m_Lexer->next();

			if (auto bodyOpt = parseTerm())
			{
				return BinOpTerm(
					op, std::move(bodyOpt.value())
				);
			}
			else
			{
				parseError("Expected term after binary operation", *m_Lexer);
			}
		}

		return BinOpTerm(
			op
		);
	}

	return std::nullopt;
}

std::optional<std::variant<CasesTerm<Prim_t>, CasesTerm<Loc_t>>> Parser::parseCases()
{
	if (m_Lexer->isPeekToken(Token::Lb))
	{
		m_Lexer->next();

		std::optional<TermOwner_t> otherwiseCaseOpt;
		CasesTerm<Prim_t>::Cases_t primCases;
		CasesTerm<Loc_t>::Cases_t locCases;

		bool isCaseRemaining = true;
		while (isCaseRemaining)
		{
			std::optional<Prim_t> primOpt;
			std::optional<Loc_t> locOpt;
			bool isOtherwise = false;

			if (m_Lexer->isPeekToken(Token::Primitive))
			{
				if (auto primStrOpt = m_Lexer->getPeekBuffer())
				{
					m_Lexer->next();
					int prim = std::stoi(primStrOpt.value());
					primOpt = prim;
				}
			}
			else if (m_Lexer->isPeekToken(Token::Id))
			{
				if (auto idOpt = m_Lexer->getPeekBuffer())
				{
					m_Lexer->next();
					locOpt = idOpt.value();
					isOtherwise = (idOpt.value() == "otherwise");
				}
			}
			else
			{
				parseError("Expected a case for cases", *m_Lexer);
			}

			if (m_Lexer->isPeekToken(Token::Arrow))
			{
				m_Lexer->next();

				if (auto termOpt = parseTerm())
				{
					if (isOtherwise)
					{
						otherwiseCaseOpt = newTerm(std::move(termOpt.value()));
					}
					else if (primOpt)
					{
						primCases[primOpt.value()] = newTerm(std::move(termOpt.value()));
					}
					else if (locOpt)
					{
						locCases[locOpt.value()] = newTerm(std::move(termOpt.value()));
					}

					if (m_Lexer->isPeekToken(Token::Comma))
					{
						m_Lexer->next();
						isCaseRemaining = true;
					}
					else
					{
						isCaseRemaining = false;
					}
				}
				else
				{
					parseError("Expected mapping term for case '" +
						(isOtherwise ? "otherwise" : (primOpt ? std::to_string(primOpt.value()) : locOpt.value())) +
						"'", *m_Lexer
					);
				}
			}
			else
			{
				parseError("Expected '->' after case '" +
					(isOtherwise ? "otherwise" : (primOpt ? std::to_string(primOpt.value()) : locOpt.value())) +
					"'", *m_Lexer
				);
			}
		}

		// TODO: valiate that only one sort of cases were found !

		if (m_Lexer->isPeekToken(Token::Rb))
		{
			m_Lexer->next();

			if (otherwiseCaseOpt)
			{
				if (m_Lexer->isPeekToken(Token::Dot))
				{
					m_Lexer->next();

					if (auto bodyOpt = parseTerm())
					{
						if (primCases.size() > 0)
						{
							return CasesTerm<Prim_t>(
								std::move(primCases), std::move(otherwiseCaseOpt.value()),
								std::move(bodyOpt.value())
							);
						}
						else if (locCases.size() > 0)
						{
							return CasesTerm<Loc_t>(
								std::move(locCases), std::move(otherwiseCaseOpt.value()),
								std::move(bodyOpt.value())
							);
						}
					}
				}

				if (primCases.size() > 0)
				{
					return CasesTerm<Prim_t>(
						std::move(primCases), std::move(otherwiseCaseOpt.value())
					);
				}
				else if (locCases.size() > 0)
				{
					return CasesTerm<Loc_t>(
						std::move(locCases), std::move(otherwiseCaseOpt.value())
					);
				}
			}
			else
			{
				parseError("Required an 'otherwise' case for cases", *m_Lexer);
			}
		}
	}

	return std::nullopt;
}