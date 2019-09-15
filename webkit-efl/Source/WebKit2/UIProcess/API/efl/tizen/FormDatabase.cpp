/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FormDatabase.h"

namespace WebKit {
PassRefPtr<FormDatabase> FormDatabase::create()
{
    return adoptRef(new FormDatabase());
}

FormDatabase::FormDatabase()
#if ENABLE(TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM)
    : m_profileFormDataRecordMapUpdated(false)
#endif // TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM
{
}

FormDatabase::~FormDatabase()
{
    if (isOpen())
        close();
}

static int const oldPasswordTableCols = 4;

static const String oldPasswordFormValuesTableName()
{
    return  String("PasswordFormValues");
}

static const String passwordFormValuesTableName()
{
    return String("NewPasswordFormValues");
}

static const String passwordFormDataTableName()
{
    return String("PasswordFormData");
}

static const String passwordNeverFormTableName()
{
    return String("PasswordNeverForm");
}

static const String candidateFormTableName()
{
    return String("CandidateForm");
}

#if ENABLE(TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM)
static const String profileFormTableName()
{
    return String("ProfileForm");
}
#endif // TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM

static void createDatabaseTables(SQLiteDatabase& db)
{
    // clear all tables and create PasswordForm and CandidateForm
    db.clearAllTables();

    String createCandidateForm = String::format("CREATE TABLE %s (name TEXT NOT NULL ON CONFLICT FAIL, value TEXT NOT NULL ON CONFLICT FAIL, UNIQUE (name, value) ON CONFLICT FAIL);",
        candidateFormTableName().utf8().data());
    if (!db.executeCommand(createCandidateForm)) {
        TIZEN_LOGE("Could not create %s table in database (%i) - %s", candidateFormTableName().utf8().data(), db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }

    // url: URL of stored form
    String createPasswordFormData = String::format("CREATE TABLE %s (url TEXT NOT NULL ON CONFLICT FAIL, UNIQUE (url) ON CONFLICT FAIL);",
        passwordFormDataTableName().utf8().data());
    if (!db.executeCommand(createPasswordFormData)) {
        TIZEN_LOGE("Could not create %s table in database (%i) - %s", passwordFormDataTableName().utf8().data(), db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }

    // urlIndex: rowid of inserted form data row on PasswordFormData table
    // name: name of input field
    // value: value of input field
    String createPasswordFormValues = String::format("CREATE TABLE %s (urlIndex INTEGER NOT NULL ON CONFLICT FAIL,name TEXT NOT NULL ON CONFLICT FAIL, type TEXT, value TEXT NOT NULL ON CONFLICT FAIL,name1 TEXT NOT NULL ON CONFLICT FAIL, type1 TEXT, value1 TEXT NOT NULL ON CONFLICT FAIL, UNIQUE (urlIndex, value) ON CONFLICT REPLACE);",
        passwordFormValuesTableName().utf8().data());
    if (!db.executeCommand(createPasswordFormValues)) {
        TIZEN_LOGE("Could not create %s table in database (%i) - %s", passwordFormValuesTableName().utf8().data(), db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }

    String createPasswordNeverForm = String::format("CREATE TABLE %s (url TEXT NOT NULL ON CONFLICT REPLACE);",
        passwordNeverFormTableName().utf8().data());
    if (!db.executeCommand(createPasswordNeverForm)) {
        TIZEN_LOGE("Could not create %s table in database (%i) - %s", passwordNeverFormTableName().utf8().data(), db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }

#if ENABLE(TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM)
    String createProfileForm = String::format("CREATE TABLE %s (profile TEXT NOT NULL ON CONFLICT REPLACE, name TEXT NOT NULL ON CONFLICT FAIL, value TEXT NOT NULL ON CONFLICT FAIL, UNIQUE (profile, name) ON CONFLICT REPLACE);",
        profileFormTableName().utf8().data());
    if (!db.executeCommand(createProfileForm)) {
        TIZEN_LOGE("Could not create Profile Form table in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
#endif // TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM
}

static bool isValidDatabase(SQLiteDatabase& db)
{
    // These tables should always be exist in a valid db
    if (!db.tableExists(candidateFormTableName()) || !db.tableExists(passwordFormDataTableName()) || !db.tableExists(passwordFormValuesTableName()) || !db.tableExists(passwordNeverFormTableName()))
        return false;
#if ENABLE(TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM)
    if (!db.tableExists(profileFormTableName()))
        return false;
#endif // TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM

    return true;
}

// readySQLiteStatement() handles two things
// 1 - If the SQLDatabase& argument is different, the statement must be destroyed and remade.  This happens when the user
//     switches to and from private browsing
// 2 - Lazy construction of the Statement in the first place, in case we've never made this query before
inline void readySQLiteStatement(OwnPtr<SQLiteStatement>& statement, SQLiteDatabase& db, const String& str)
{
    if (statement && (statement->database() != &db || statement->isExpired())) {
        if (statement->isExpired())
            LOG_ERROR("SQLiteStatement associated with %s is expired", str.ascii().data());
        statement.clear();
    }
    if (!statement) {
        statement = adoptPtr(new SQLiteStatement(db, str));
        if (statement->prepare() != SQLResultOk)
            LOG_ERROR("Preparing statement %s failed", str.ascii().data());
    }
}

bool FormDatabase::open(const String& directory, const String& filename)
{
    if (isOpen()) {
        LOG_ERROR("Attempt to reopen FormDatabase");
        return false;
    }

    m_databaseDirectoryPath = directory;
    m_databaseFilePath = pathByAppendingComponent(m_databaseDirectoryPath, filename);
    makeAllDirectories(m_databaseDirectoryPath);

    if (!m_syncFormDatabase.open(m_databaseFilePath)) {
        LOG_ERROR("Unable to open form database at path %s - %s", m_databaseFilePath.ascii().data(), m_syncFormDatabase.lastErrorMsg());
        return false;
    }

    if(m_syncFormDatabase.tableExists(oldPasswordFormValuesTableName())) {
        // This is to migrate already saved passwords to newPasswordValuesTable when fota upgrade is done.
        // There are changes in schema of table passwordFormValuesTable.
        // So this branch is executed only once . Once migration is completed old passwordFormValues table is dropped.
        readySQLiteStatement(m_selectPasswordFormValuesStatement, m_syncFormDatabase,
        String::format("SELECT * FROM %s;", oldPasswordFormValuesTableName().utf8().data()));

        int resultValues = m_selectPasswordFormValuesStatement->step();
        int columns = static_cast<int>(m_selectPasswordFormValuesStatement->columnCount());
        if(columns == oldPasswordTableCols) {
            Vector<WebFormData::Data::ValueData> passwordFormData;
            int urlIndex1,urlIndex2;
            int insertResultValues;
            String createNewPasswordFormValues = String::format("CREATE TABLE %s (urlIndex INTEGER NOT NULL ON CONFLICT FAIL,name TEXT NOT NULL ON CONFLICT FAIL, type TEXT, value TEXT NOT NULL ON CONFLICT FAIL,name1 TEXT NOT NULL ON CONFLICT FAIL, type1 TEXT, value1 TEXT NOT NULL ON CONFLICT FAIL, UNIQUE (urlIndex, value) ON CONFLICT REPLACE);",
                                                 passwordFormValuesTableName().utf8().data());
            if (!m_syncFormDatabase.executeCommand(createNewPasswordFormValues)) {
                TIZEN_LOGE("Could not create %s table in database (%i) - %s", passwordFormValuesTableName().utf8().data(), m_syncFormDatabase.lastError(), m_syncFormDatabase.lastErrorMsg());
                m_syncFormDatabase.close();
                return false;
            }

            while (resultValues == SQLResultRow) {
                urlIndex1 = static_cast<int>(m_selectPasswordFormValuesStatement->getColumnInt(0));
                String name = m_selectPasswordFormValuesStatement->getColumnText(1);
                String type = m_selectPasswordFormValuesStatement->getColumnText(2);
                String value = m_selectPasswordFormValuesStatement->getColumnText(3);
                resultValues = m_selectPasswordFormValuesStatement->step();
                if(resultValues == SQLResultRow) {
                    urlIndex2 = static_cast<int>(m_selectPasswordFormValuesStatement->getColumnInt(0));
                    if(urlIndex1 == urlIndex2) {
                        String name1 = m_selectPasswordFormValuesStatement->getColumnText(1);
                        String type1 = m_selectPasswordFormValuesStatement->getColumnText(2);
                        String value1 = m_selectPasswordFormValuesStatement->getColumnText(3);
                        readySQLiteStatement(m_insertNewPasswordFormValuesStatement, m_syncFormDatabase,
                        String::format("INSERT INTO %s (urlIndex, name, type, value, name1, type1, value1) VALUES (?, ?, ?, ?, ?, ?, ?);", passwordFormValuesTableName().utf8().data()));
                        m_insertNewPasswordFormValuesStatement->bindInt(1, urlIndex1);
                        m_insertNewPasswordFormValuesStatement->bindText(2, name);
                        m_insertNewPasswordFormValuesStatement->bindText(3, type);
                        m_insertNewPasswordFormValuesStatement->bindText(4, value);
                        m_insertNewPasswordFormValuesStatement->bindText(5, name1);
                        m_insertNewPasswordFormValuesStatement->bindText(6, type1);
                        m_insertNewPasswordFormValuesStatement->bindText(7, value1);
                        insertResultValues = m_insertNewPasswordFormValuesStatement->step();
                        if(insertResultValues != SQLResultDone){
                            TIZEN_LOGE("Could not migrate %s table in database (%i) - %s", passwordFormValuesTableName().utf8().data(), m_syncFormDatabase.lastError(), m_syncFormDatabase.lastErrorMsg());
                            m_syncFormDatabase.close();
                            return false;
                        }
                        resultValues = m_selectPasswordFormValuesStatement->step();
                        m_insertNewPasswordFormValuesStatement->reset();
                    }
                }
            }
            m_selectPasswordFormValuesStatement->reset();
            readySQLiteStatement(m_dropPasswordFormValuesStatement, m_syncFormDatabase,
            String::format("DROP TABLE %s;", oldPasswordFormValuesTableName().utf8().data()));
            int resultValue = m_dropPasswordFormValuesStatement->step();
            if(resultValue != SQLResultDone){
                TIZEN_LOGE("Could not drop %s table in database (%i) - %s", oldPasswordFormValuesTableName().utf8().data(), m_syncFormDatabase.lastError(), m_syncFormDatabase.lastErrorMsg());
                m_syncFormDatabase.close();
                return false;
            }
        }
    }
    if (!isValidDatabase(m_syncFormDatabase)) {
        LOG_ERROR("Invalid form database");
        m_syncFormDatabase.clearAllTables();
        createDatabaseTables(m_syncFormDatabase);
        if (!isOpen()) {
            LOG_ERROR("Invalid database and failed to recreate tables");
            return false;
        }
    }

#if ENABLE(TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM)
    else
        initProfileFormData();
#endif // TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM
    return true;
}

void FormDatabase::close()
{
    m_syncFormDatabase.close();
}

bool FormDatabase::isOpen() const
{
    return m_syncFormDatabase.isOpen();
}

String FormDatabase::databasePath() const
{
    return m_databaseFilePath.isolatedCopy();
}

String FormDatabase::defaultDatabaseDirectoryPath()
{
    return WebCore::pathByAppendingComponent(WebCore::homeDirectoryPath(), ".webkit/formDatabase/");
}

String FormDatabase::defaultDatabaseFilename()
{
    DEFINE_STATIC_LOCAL(String, defaultDatabaseFilename, ("WebpageForms.db"));
    return defaultDatabaseFilename.isolatedCopy();
}


bool FormDatabase::getPasswordFormDataForURL(const String& url, int& urlIndex, String& formID, String& sourceFrameURL, bool& useFingerprint, String& buttonID)
{
    readySQLiteStatement(m_selectPasswordFormDataForURLStatement, m_syncFormDatabase,
        String::format("SELECT rowid FROM %s WHERE url=(?);", passwordFormDataTableName().utf8().data()));

    m_selectPasswordFormDataForURLStatement->bindText(1, url);

    int result = m_selectPasswordFormDataForURLStatement->step();

    int urlIndexTemp = static_cast<int>(m_selectPasswordFormDataForURLStatement->getColumnInt(0));
    String formIDTemp;
    String sourceFrameURLTemp;
    bool useFingerprintTemp = false;
    String buttonIDTemp;

    m_selectPasswordFormDataForURLStatement->reset();

    if (result != SQLResultRow) {
        TIZEN_LOGE("Error on selecting formdata from database");
        return false;
    }

    // No form data row
    if (!urlIndexTemp)
        return false;

    urlIndex = urlIndexTemp;
    formID = formIDTemp;
    sourceFrameURL = sourceFrameURLTemp;
    useFingerprint = useFingerprintTemp;
    buttonID = buttonIDTemp;

    return true;
}

bool FormDatabase::getPasswordFormValuesForURL(const String& url, String& formID, String& sourceFrameURL, bool& useFingerprint, String& buttonID, Vector<WebFormData::Data::ValueData>& loginFormDataVector)
{

    int urlIndex = 0;
    if (!getPasswordFormDataForURL(url, urlIndex, formID, sourceFrameURL, useFingerprint, buttonID))
        return false;
    readySQLiteStatement(m_selectPasswordFormValuesForURLStatement, m_syncFormDatabase,
        String::format("SELECT name, type, value, name1, type1, value1 FROM %s WHERE urlIndex=(?);", passwordFormValuesTableName().utf8().data()));
    m_selectPasswordFormValuesForURLStatement->bindInt(1, urlIndex);

    int resultValues = m_selectPasswordFormValuesForURLStatement->step();
    while (resultValues == SQLResultRow) {
        String name = m_selectPasswordFormValuesForURLStatement->getColumnText(0);
        String type = m_selectPasswordFormValuesForURLStatement->getColumnText(1);
        String value = m_selectPasswordFormValuesForURLStatement->getColumnText(2);
        String name1 = m_selectPasswordFormValuesForURLStatement->getColumnText(3);
        String type1 = m_selectPasswordFormValuesForURLStatement->getColumnText(4);
        String value1 = m_selectPasswordFormValuesForURLStatement->getColumnText(5);
        if (name.length() && type.length() && value.length()) {
            loginFormDataVector.append(WebFormData::Data::ValueData(type, name, value, String(), false));
            loginFormDataVector.append(WebFormData::Data::ValueData(type1, name1, value1, String(), false));
        }

        resultValues = m_selectPasswordFormValuesForURLStatement->step();
    }

    m_selectPasswordFormValuesForURLStatement->reset();

    if (resultValues != SQLResultDone) {
        LOG_ERROR("Error on selecting candidate formdata from database");
        loginFormDataVector.clear();
        return false;
    }

    m_lastPasswordFormURL = url;
    m_lastPasswordFormValues = loginFormDataVector;

    if (!loginFormDataVector.size())
        return false;

    return true;
}

bool FormDatabase::getPasswordNeverURLs( Vector<String >& urls)
{
    // Load Urls
    readySQLiteStatement(m_selectPasswordNeverURLStatement, m_syncFormDatabase,
        String::format("SELECT * FROM %s ;", passwordNeverFormTableName().utf8().data()));

    int result = m_selectPasswordNeverURLStatement->step();
    while (result == SQLResultRow) {
        String uri = m_selectPasswordNeverURLStatement->getColumnText(0);
        if (uri.length())
            urls.append(uri);
        result = m_selectPasswordNeverURLStatement->step();
    }

    m_selectPasswordNeverURLStatement->reset();

    if (result != SQLResultDone) {
        LOG_ERROR("Error on selecting candidate formdata from database");
        urls.clear();
        return false;
    }

    if (!urls.size())
        return false;

    return true;
}

void FormDatabase::addPasswordFormData(const String& url, WKFormDataRef formData, bool useFingerprint)
{
    if (!isOpen()) {
        LOG_ERROR("Form database is not opened yet");
        return;
    }

    WebFormData* data = toImpl(formData);
    int urlIndex;

    readySQLiteStatement(m_selectPasswordFormDataForURLStatement, m_syncFormDatabase,
        String::format("SELECT rowid FROM %s WHERE url=(?);", passwordFormDataTableName().utf8().data()));
    m_selectPasswordFormDataForURLStatement->bindText(1, url);
    int result = m_selectPasswordFormDataForURLStatement->step();
    urlIndex = static_cast<int>(m_selectPasswordFormDataForURLStatement->getColumnInt(0));
    m_selectPasswordFormDataForURLStatement->reset();

    if(!urlIndex) {
        readySQLiteStatement(m_insertPasswordFormDataStatement, m_syncFormDatabase,
            String::format("INSERT INTO %s (url) VALUES (?);", passwordFormDataTableName().utf8().data()));
        m_insertPasswordFormDataStatement->bindText(1, url);
        int resultForm = m_insertPasswordFormDataStatement->step();
        urlIndex = static_cast<int>(m_syncFormDatabase.lastInsertRowID());
        m_insertPasswordFormDataStatement->reset();

        if (resultForm != SQLResultDone) {
           #if ENABLE(TIZEN_DLOG_SUPPORT)
               TIZEN_LOGE("%s add failed", passwordFormDataTableName().utf8().data());
           #endif
           return;
       }
   }

    m_lastPasswordFormValues.clear();
    size_t size = data->values().size();
    if(size%2==0) {
        for (size_t i = 0; i < size/2; i++) {
            if (data->values()[i].m_name.isEmpty() || data->values()[i].m_value.isEmpty())
                continue;
            if (data->values()[i+1].m_name.isEmpty() || data->values()[i+1].m_value.isEmpty())
                continue;

            m_lastPasswordFormValues.append(
                WebFormData::Data::ValueData(data->values()[i].m_type.isEmpty() ? "text" : data->values()[i].m_type, data->values()[i].m_name, data->values()[i].m_value, String(), false));
            m_lastPasswordFormValues.append(
                WebFormData::Data::ValueData(data->values()[i+1].m_type.isEmpty() ? "text" : data->values()[i+1].m_type, data->values()[i+1].m_name, data->values()[i+1].m_value, String(), false));

            readySQLiteStatement(m_insertPasswordFormValuesStatement, m_syncFormDatabase,
                String::format("INSERT INTO %s (urlIndex, name, type, value, name1, type1, value1) VALUES (?, ?, ?, ?, ?, ?, ?);",passwordFormValuesTableName().utf8().data()));
            m_insertPasswordFormValuesStatement->bindInt(1, urlIndex);
            m_insertPasswordFormValuesStatement->bindText(2, data->values()[i].m_name);
            m_insertPasswordFormValuesStatement->bindText(3, data->values()[i].m_type);
            m_insertPasswordFormValuesStatement->bindText(4, data->values()[i].m_value);
            m_insertPasswordFormValuesStatement->bindText(5, data->values()[i+1].m_name);
            m_insertPasswordFormValuesStatement->bindText(6, data->values()[i+1].m_type);
            m_insertPasswordFormValuesStatement->bindText(7, data->values()[i+1].m_value);
            int resultValues = m_insertPasswordFormValuesStatement->step();
            m_insertPasswordFormValuesStatement->reset();

            if (resultValues != SQLResultDone) {
                #if ENABLE(TIZEN_DLOG_SUPPORT)
                    TIZEN_LOGE("%s add failed", passwordFormValuesTableName().utf8().data());
                #endif
                m_lastPasswordFormValues.clear();
                return;
           }
        }
    }
    m_lastPasswordFormURL = url;
}

bool FormDatabase::addURLForPasswordNever(const String& url)
{
    if (!isOpen()) {
        LOG_ERROR("Form database is not opened yet");
        return false;
    }

    readySQLiteStatement(m_insertPasswordNeverDataStatement, m_syncFormDatabase,
        String::format("INSERT INTO %s (url) VALUES (?);", passwordNeverFormTableName().utf8().data()));
    m_insertPasswordNeverDataStatement->bindText(1, url);

    int result = m_insertPasswordNeverDataStatement->step();
    m_insertPasswordNeverDataStatement->reset();
    if (result != SQLResultDone) {
        LOG_ERROR("%s add failed: [%s] ", passwordNeverFormTableName().utf8().data(), url.utf8().data());
        return false;
    }

    return true;
}


bool FormDatabase::getCandidateFormDataForName(const String& name, Vector<String>& candidateFormData)
{
    // If last candidate form name is same to requested one, return the last candidate form data
    if (name == m_lastCandidateFormName) {
        candidateFormData = m_lastCandidateFormData;
        if (candidateFormData.size())
            return true;
        return false;
    }

    // Load CandidateForm data pairs
    readySQLiteStatement(m_selectCandidateFormDataForNameStatement, m_syncFormDatabase,
        String::format("SELECT value FROM %s WHERE name=(?);", candidateFormTableName().utf8().data()));
    m_selectCandidateFormDataForNameStatement->bindText(1, name);

    int result = m_selectCandidateFormDataForNameStatement->step();
    while (result == SQLResultRow) {
        String value = m_selectCandidateFormDataForNameStatement->getColumnText(0);
        if (value.length())
            candidateFormData.append(value);
        result = m_selectCandidateFormDataForNameStatement->step();
    }

    m_selectCandidateFormDataForNameStatement->reset();

    if (result != SQLResultDone) {
        LOG_ERROR("Error on selecting candidate formdata from database");
        candidateFormData.clear();
        return false;
    }

    m_lastCandidateFormName = name;
    m_lastCandidateFormData = candidateFormData;

    if (!candidateFormData.size())
        return false;

    return true;
}

void FormDatabase::addCandidateFormData(WKFormDataRef formDataRef)
{
    if (!isOpen()) {
        LOG_ERROR("Form database is not opened yet");
        return;
    }

    WebFormData* formData = toImpl(formDataRef);

    size_t size = formData->values().size();
    for (size_t i = 0; i < size; i++) {
        if (!formData->values()[i].m_shouldAutocomplete)
            continue;

        if (formData->containsPassword() && equalIgnoringCase(formData->values()[i].m_type, "password"))
            continue;

        if (formData->values()[i].m_value.isEmpty())
            continue;

        readySQLiteStatement(m_insertCandidateFormDataStatement, m_syncFormDatabase,
            String::format("INSERT INTO %s (name, value) VALUES (?, ?);", candidateFormTableName().utf8().data()));
        m_insertCandidateFormDataStatement->bindText(1, formData->values()[i].m_name);
        m_insertCandidateFormDataStatement->bindText(2, formData->values()[i].m_value);

        int result = m_insertCandidateFormDataStatement->step();
        m_insertCandidateFormDataStatement->reset();
        if (result == SQLResultDone) {
            if (m_lastCandidateFormName == formData->values()[i].m_name)
                m_lastCandidateFormData.append(formData->values()[i].m_value);
        } else
            LOG_ERROR("%s add failed: %s", candidateFormTableName().utf8().data(),
                formData->values()[i].m_name.utf8().data());
    }
}

void FormDatabase::clearCandidateFormData()
{
    if (!isOpen())
        return;

    m_syncFormDatabase.executeCommand(String::format("DELETE FROM %s;", candidateFormTableName().utf8().data()));
    m_lastCandidateFormName = emptyString();
    m_lastCandidateFormData.clear();
}

void FormDatabase::clearPasswordFormData()
{
    if (!isOpen())
        return;

    m_syncFormDatabase.executeCommand(String::format("DELETE FROM %s;", passwordFormDataTableName().utf8().data()));
    m_syncFormDatabase.executeCommand(String::format("DELETE FROM %s;", passwordFormValuesTableName().utf8().data()));
    m_lastPasswordFormValues.clear();
    m_lastPasswordFormURL = emptyString();

    m_syncFormDatabase.executeCommand(String::format("DELETE FROM %s;", passwordNeverFormTableName().utf8().data()));
}

void FormDatabase::clearPasswordFormDataForURL(const String& url)
{
    if (!isOpen())
        return;

    readySQLiteStatement(m_deletePasswordFormValuesForURLStatement, m_syncFormDatabase,
        String::format("DELETE FROM %s WHERE urlIndex in (SELECT rowid FROM %s WHERE url=(?));",
            passwordFormValuesTableName().utf8().data(), passwordFormDataTableName().utf8().data()));

    m_deletePasswordFormValuesForURLStatement->bindText(1, url);

    int result = m_deletePasswordFormValuesForURLStatement->step();
    m_deletePasswordFormValuesForURLStatement->reset();
#if ENABLE(TIZEN_DLOG_SUPPORT)
    if (result != SQLResultDone)
        TIZEN_LOGE("DELETE values failed");
#endif

    readySQLiteStatement(m_deletePasswordFormDataForURLStatement, m_syncFormDatabase,
        String::format("DELETE FROM %s WHERE url=(?);",
        passwordFormDataTableName().utf8().data()));
    m_deletePasswordFormDataForURLStatement->bindText(1, url);

    result = m_deletePasswordFormDataForURLStatement->step();
    m_deletePasswordFormDataForURLStatement->reset();
#if ENABLE(TIZEN_DLOG_SUPPORT)
    if (result != SQLResultDone)
        TIZEN_LOGE("DELETE data failed");
#endif

    if (m_lastPasswordFormURL != url)
        return;

    m_lastPasswordFormValues.clear();
    m_lastPasswordFormURL = emptyString();
}

void FormDatabase::getAllPasswordFormData(Vector<FormDatabase::PasswordFormData>& vector)
{
    readySQLiteStatement(m_selectAllPasswordFormDataStatement, m_syncFormDatabase,
        String::format("SELECT url, useFingerprint FROM %s ;", passwordFormDataTableName().utf8().data()));

    int result = m_selectAllPasswordFormDataStatement->step();
    while (result == SQLResultRow) {
        PasswordFormData data;
        data.m_url = m_selectAllPasswordFormDataStatement->getColumnText(0);
        int useFingerprint = m_selectAllPasswordFormDataStatement->getColumnInt(1);
        data.m_useFingerprint = useFingerprint ? true : false;

        vector.append(data);

        result = m_selectAllPasswordFormDataStatement->step();
    }

    m_selectAllPasswordFormDataStatement->reset();

    if (result != SQLResultDone) {
        LOG_ERROR("Failed");
        vector.clear();
    }
}

#if ENABLE(TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM)
bool FormDatabase::addProfileFormDataToMap(const String& profile, const String& name, const String& value)
{
    HashSet<std::pair<String, String> > existingProfileFormData = m_profileFormDataRecordMap.get(profile);
    HashSet<std::pair<String, String> >::iterator end = existingProfileFormData.end();

    for (HashSet<std::pair<String, String> >::iterator it = existingProfileFormData.begin(); it != end; ++it) {
        if(name == it->first)
            existingProfileFormData.remove(it);
    }

    existingProfileFormData.add(std::pair<String, String>(name, value));
    m_profileFormDataRecordMap.remove(profile);
    m_profileFormDataRecordMap.set(profile,existingProfileFormData);
    return true;
}

bool FormDatabase::addProfileFormData(const String& profile, Vector<std::pair<String, String> >& textFormData)
{
    if (!isOpen()) {
        LOG_ERROR("Form database is not opened yet");
        return false;
    }

    size_t size = textFormData.size();
    for (size_t i = 0; i < size; i++) {
        if (!textFormData[i].second.length())
            continue;

        bool isNewRecord = true;
        readySQLiteStatement(m_insertProfileFormDataStatement, m_syncFormDatabase, String::format("INSERT INTO %s (profile, name, value) VALUES (?, ?, ?);", profileFormTableName().utf8().data()));
        m_insertProfileFormDataStatement->bindText(1, profile);
        m_insertProfileFormDataStatement->bindText(2, textFormData[i].first);
        m_insertProfileFormDataStatement->bindText(3, textFormData[i].second);
        if (isNewRecord) {
            int result = m_insertProfileFormDataStatement->step();
            m_insertProfileFormDataStatement->reset();
            if (result == SQLResultDone) {
                addProfileFormDataToMap(profile, textFormData[i].first, textFormData[i].second);
                m_profileFormDataRecordMapUpdated = true;
            } else {
                LOG_ERROR("Text add failed: %s, %s", textFormData[i].first.utf8().data(), textFormData[i].second.utf8().data());
                return false;
            }
        }
    }
    return true;
}

bool FormDatabase::getProfileFormDataForProfile(const String& profile, Vector<std::pair<String, String> >& textFormData)
{
    HashSet<std::pair<String, String> > existingTextFormData = m_profileFormDataRecordMap.get(profile);
    if (!existingTextFormData.size())
        return false;

    HashSet<std::pair<String, String> >::iterator end = existingTextFormData.end();
    for (HashSet<std::pair<String, String> >::iterator it = existingTextFormData.begin(); it != end; ++it)
        textFormData.append(std::pair<String, String>(it->first, it->second));

    return true;
}

void FormDatabase::initProfileFormData()
{
    readySQLiteStatement(m_selectProfileFormDataForProfileStatement, m_syncFormDatabase, String::format("SELECT profile, name, value FROM %s;", profileFormTableName().utf8().data()));

    int result = m_selectProfileFormDataForProfileStatement->step();
    while (result == SQLResultRow) {
        String profile = m_selectProfileFormDataForProfileStatement->getColumnText(0);
        String name = m_selectProfileFormDataForProfileStatement->getColumnText(1);
        String value = m_selectProfileFormDataForProfileStatement->getColumnText(2);

        if (profile.length() && name.length() && value.length())
            addProfileFormDataToMap(profile, name, value);
        result = m_selectProfileFormDataForProfileStatement->step();
    }

    if (result != SQLResultDone)
        LOG_ERROR("Error reading Profile Form data from database");

    m_selectProfileFormDataForProfileStatement->reset();
    m_profileFormDataRecordMapUpdated = true;
}

void FormDatabase::getAllProfileIDForProfileFormData(Vector<String>& list)
{
    OwnPtr<WebCore::SQLiteStatement> selectDistinctProfileFormDataStatement;
    readySQLiteStatement(selectDistinctProfileFormDataStatement, m_syncFormDatabase, String::format("SELECT DISTINCT profile FROM %s;", profileFormTableName().utf8().data()));
    int result = selectDistinctProfileFormDataStatement->step();
    while (result == SQLResultRow) {
        String profileID = selectDistinctProfileFormDataStatement->getColumnText(0);
        list.append(profileID);
        result = selectDistinctProfileFormDataStatement->step();
    }
    selectDistinctProfileFormDataStatement->reset();
}

void FormDatabase::getProfileFormDataMap(HashMap<String, HashSet<std::pair<String, String> > > &getFormData)
{
    getFormData = m_profileFormDataRecordMap;
}

bool FormDatabase::clearProfileFormData(String& profileID)
{
    if (!isOpen())
        return false;

    bool ret = m_syncFormDatabase.executeCommand(String::format("DELETE FROM %s WHERE profile=\"%s\";", profileFormTableName().utf8().data(), profileID.utf8().data()));
    if(ret) {
        m_profileFormDataRecordMap.remove(profileID);
        m_profileFormDataRecordMapUpdated = true;
        return true;
    }

    return false;
}

int FormDatabase::getNextIdForProfileFormData()
{
    int maxId = 0;
    if (m_profileFormDataRecordMap.isEmpty())
        return maxId;

    HashMap<String, HashSet<std::pair<String, String> > >::iterator end = m_profileFormDataRecordMap.end();

    for (HashMap<String, HashSet<std::pair<String, String> > >::iterator it = m_profileFormDataRecordMap.begin(); it != end; ++it) {
        int localMaxId = atoi(it->first.utf8().data());
        maxId = (maxId>localMaxId ? maxId : localMaxId);
    }
    maxId++;
    return maxId;
}

void FormDatabase::getAuxStringForDisplay(const String& profileID, const String& mainString, std::pair<String, String>& auxStringPair)
{
    HashSet<std::pair<String, String> > existingProfileFormData = m_profileFormDataRecordMap.get(profileID);
    HashSet<std::pair<String, String> >::iterator end = existingProfileFormData.end();
    String auxStringName = auxStringPair.first;
    String auxStringvalue;
    for (HashSet<std::pair<String, String> >::iterator it = existingProfileFormData.begin(); it != end; ++it) {
        if (auxStringName.isEmpty() && mainString != it->second) {
            auxStringPair = make_pair(it->first, it->second);
            return;
        }
        if (mainString != it->second) {
            if (!auxStringName.isEmpty() && auxStringName == it->first) {
                auxStringPair.second = it->second;
                return;
            }
            if (auxStringvalue.isEmpty())
                auxStringvalue = it->second;
        }
    }
    if (!auxStringvalue.isEmpty()) {
        auxStringPair = make_pair(auxStringName, auxStringvalue);
        return;
    }
    auxStringPair = make_pair(auxStringName, String());
}

#endif // TIZEN_WEBKIT2_AUTOFILL_PROFILE_FORM

}
