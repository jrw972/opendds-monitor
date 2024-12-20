#include "dds_manager.h"
#include "dds_data.h"
#include "qos_dictionary.h"
#include "open_dynamic_data.h"

#include <QMutexLocker>

#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/XTypes/Utils.h>

#include <tao/AnyTypeCode/Any.h>

#include <iostream>

std::unique_ptr<DDSManager> CommonData::m_ddsManager;
QMap<QString, QList<std::shared_ptr<OpenDynamicData> > > CommonData::m_samples;
QMap<QString, QStringList> CommonData::m_sampleTimes;
QMap<QString, std::shared_ptr<TopicInfo>> CommonData::m_topicInfo;
QMap<QString, QList<DDS::DynamicData_var> > CommonData::m_dynamicSamples;
QMutex CommonData::m_sampleMutex;
QMutex CommonData::m_topicMutex;
QMutex CommonData::m_dynamicSamplesMutex;


//------------------------------------------------------------------------------
void CommonData::cleanup()
{
    //QMap<QString, std::shared_ptr<TopicInfo>>::iterator topicIter;
    //for (topicIter = m_topicInfo.begin();
    //     topicIter != m_topicInfo.end();
    //     ++topicIter)
    //{
    //    delete topicIter.value();
    //    topicIter.value() = nullptr;
    //}
    {
        QMutexLocker locker(&m_sampleMutex);
        m_samples.clear();
        m_sampleTimes.clear();
    }

    {
        QMutexLocker locker(&m_topicMutex);
        m_topicInfo.clear();
    }

    // Deleting m_ddsManager causes the shared memory transport to crash when
    // closing the DDS service, so attempt leave the domain ourself.
    //delete CommonData::m_ddsManager;
    //DDS::DomainParticipant* domain = CommonData::m_ddsManager->getDomainParticipant();
    //DDS::DomainParticipantFactory_var dpf = TheParticipantFactory;
    //if (domain && !CORBA::is_nil(dpf.in()))
    //{
    //    domain->delete_contained_entities();
    //    dpf->delete_participant(domain);
    //}
    //CommonData::m_ddsManager = nullptr;
}


//------------------------------------------------------------------------------
void CommonData::storeTopicInfo(const QString& topicName, std::shared_ptr<TopicInfo> info)
{
    QMutexLocker locker(&m_topicMutex);
    m_topicInfo[topicName] = info;
}

//------------------------------------------------------------------------------
std::shared_ptr<TopicInfo> CommonData::getTopicInfo(const QString& topicName)
{
    std::shared_ptr<TopicInfo> topicInfo;
    QMutexLocker locker(&m_topicMutex);

    if (m_topicInfo.contains(topicName))
    {
        topicInfo = m_topicInfo.value(topicName);
    }

    return topicInfo;
}

QVariant CommonData::readMember(const QString& topicName,
                                const QString& memberName,
                                unsigned int index)
{
    QVariant value;
    QMutexLocker locker(&m_sampleMutex);

    // Make sure the index is valid
    QList<std::shared_ptr<OpenDynamicData>>& sampleList = m_samples[topicName];
    if ((int)index >= sampleList.count())
    {
        value = "NULL";
        return value;
    }

    const std::shared_ptr<OpenDynamicData> targetSample = sampleList.at(index);
    if (!targetSample)
    {
        value = "NULL";
        return value;
    }

    // Find the target member within this sample
    const std::shared_ptr<OpenDynamicData> targetMember =
        targetSample->getMember(memberName.toUtf8().data());

    if (!targetMember)
    {
        value = "NULL";
        return value;
    }

    // Store the value into a QVariant
    // The tmpValue may seem redundant, but it's very helpful for debug
    CORBA::TCKind type = targetMember->getKind();
    switch (type)
    {
    case CORBA::tk_long:
    {
        int32_t tmpValue = targetMember->getValue<int32_t>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_short:
    {
        int16_t tmpValue = targetMember->getValue<int16_t>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_ushort:
    {
        uint16_t tmpValue = targetMember->getValue<uint16_t>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_ulong:
    {
        uint32_t tmpValue = targetMember->getValue<uint32_t>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_float:
    {
        float tmpValue = targetMember->getValue<float>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_double:
    {
        double tmpValue = targetMember->getValue<double>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_boolean:
    {
        uint32_t tmpValue = targetMember->getValue<uint32_t>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_char:
    {
        char tmpValue = targetMember->getValue<char>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_wchar:
    {
        char tmpValue = targetMember->getValue<char>(); // FIXME?
        value = tmpValue;
        break;
    }
    case CORBA::tk_octet:
    {
        uint8_t tmpValue = targetMember->getValue<uint8_t>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_longlong:
    {
        qint64 tmpValue = targetMember->getValue<qint64>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_ulonglong:
    {
        quint64 tmpValue = targetMember->getValue<quint64>();
        value = tmpValue;
        break;
    }
    case CORBA::tk_string:
    {
        const char* tmpValue = targetMember->getStringValue();
        value = tmpValue;
        break;
    }
    case CORBA::tk_enum:
    {
        const uint32_t tmpValue = targetMember->getValue<uint32_t>();
        value = tmpValue;
        break;
    }
    default:
        value = "NULL";
        break;

    } // End targetMember type switch

    return value;
}

QVariant CommonData::readDynamicMember(const QString& topicName,
                                       const QString& memberName,
                                       unsigned int index)
{
    const QVariant error;
    QMutexLocker locker(&m_dynamicSamplesMutex);

    if (!m_dynamicSamples.contains(topicName)) {
        return error;
    }

    const QList<DDS::DynamicData_var>& sampleList = m_dynamicSamples[topicName];
    if (static_cast<int>(index) >= sampleList.count()) {
        return error;
    }

    DDS::DynamicData_var sample = sampleList.at(index);
    DDS::DynamicType_var topic_type = sample->type();
    OpenDDS::XTypes::MemberPath member_path;
    if (member_path.resolve_string_path(topic_type, memberName.toStdString()) != DDS::RETCODE_OK) {
        return error;
    }

    // The direct parent dynamic data of this member
    DDS::DynamicData_var parent_data;

    // The Id of this member within the direct parent type
    DDS::MemberId id;

    if (member_path.get_member_from_data(sample, parent_data, id) != DDS::RETCODE_OK) {
        return error;
    }

    DDS::DynamicType_var parent_type = parent_data->type();
    DDS::DynamicTypeMember_var dtm;
    if (parent_type->get_member(dtm, id) != DDS::RETCODE_OK) {
        return error;
    }

    DDS::MemberDescriptor_var md;
    if (dtm->get_descriptor(md) != DDS::RETCODE_OK) {
        return error;
    }

    const DDS::TypeKind member_tk = md->type()->get_kind();
    DDS::ReturnCode_t rc = DDS::RETCODE_OK;

    switch (member_tk) {
    case OpenDDS::XTypes::TK_BOOLEAN:
      {
          CORBA::Boolean tmp;
          rc = parent_data->get_boolean_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_BYTE:
      {
          CORBA::Octet tmp;
          rc = parent_data->get_byte_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_INT16:
      {
          CORBA::Short tmp;
          rc = parent_data->get_int16_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_UINT16:
      {
          CORBA::UShort tmp;
          rc = parent_data->get_uint16_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_INT32:
      {
          CORBA::Long tmp;
          rc = parent_data->get_int32_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_UINT32:
      {
          CORBA::ULong tmp;
          rc = parent_data->get_uint32_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_INT64:
      {
          CORBA::LongLong tmp;
          rc = parent_data->get_int64_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return static_cast<qlonglong>(tmp);
          }
          break;
      }
    case OpenDDS::XTypes::TK_UINT64:
      {
          CORBA::ULongLong tmp;
          rc = parent_data->get_uint64_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return static_cast<qulonglong>(tmp);
          }
          break;
      }
    case OpenDDS::XTypes::TK_FLOAT32:
      {
          CORBA::Float tmp;
          rc = parent_data->get_float32_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_FLOAT64:
      {
          CORBA::Double tmp;
          rc = parent_data->get_float64_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_CHAR8:
      {
          CORBA::Char tmp;
          rc = parent_data->get_char8_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_CHAR16:
      {
          CORBA::WChar tmp;
          rc = parent_data->get_char16_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp;
          }
          break;
      }
    case OpenDDS::XTypes::TK_STRING8:
      {
          CORBA::String_var tmp;
          rc = parent_data->get_string_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              return tmp.in();
          }
          break;
      }
    case OpenDDS::XTypes::TK_ENUM:
      {
          CORBA::Long tmp;
          rc = parent_data->get_int32_value(tmp, id);
          if (rc == DDS::RETCODE_OK) {
              DDS::String8_var name;
              rc = OpenDDS::XTypes::get_enumerator_name(name, tmp, md->type());
              if (rc == DDS::RETCODE_OK) {
                  return name.in();
              }
          }
          break;
      }
    default:
        break;
    }

    return error;
}

//------------------------------------------------------------------------------
QVariant CommonData::readValue(const QString& topicName,
                               const QString& memberName,
                               unsigned int index)
{
    const QVariant value = readDynamicMember(topicName, memberName, index);
    if (!value.isNull()) {
        return value;
    }
    return readMember(topicName, memberName, index);
}


//------------------------------------------------------------------------------
void CommonData::flushSamples(const QString& topicName)
{
    QMutexLocker locker(&m_sampleMutex);

    //Confirm this still does the same thing -MM
    //auto sampleList = m_samples[topicName];

    //for (int i = 0; i < sampleList.size(); i++)
    //{
    //   delete sampleList[i];
    //   sampleList[i] = nullptr;
    //}

    m_samples[topicName].clear();
    m_sampleTimes[topicName].clear();
}


//------------------------------------------------------------------------------
void CommonData::storeSample(const QString& topicName,
                             const QString& sampleName,
                             const std::shared_ptr<OpenDynamicData> sample)
{
    QMutexLocker locker(&m_sampleMutex);

    QList<std::shared_ptr<OpenDynamicData>>& sampleList = m_samples[topicName];
    QStringList& timesList = m_sampleTimes[topicName];

    // Store a pointer to the new sample
    sampleList.push_front(sample);
    timesList.push_front(sampleName);

    // Cleanup
    while (sampleList.size() > MAX_SAMPLES)
    {
       //delete sampleList.back();
       //sampleList.back() = nullptr;
       sampleList.pop_back();
       timesList.pop_back();
    }
}

//------------------------------------------------------------------------------
void CommonData::storeDynamicSample(const QString& topic_name,
                                    const QString& sample_name,
                                    const DDS::DynamicData_var sample)
{
    QMutexLocker locker(&m_dynamicSamplesMutex);

    QList<DDS::DynamicData_var>& sample_list = m_dynamicSamples[topic_name];
    QStringList& times_list = m_sampleTimes[topic_name];

    // Add new sample
    sample_list.push_front(sample);
    times_list.push_front(sample_name);

    // Cleanup
    while (sample_list.size() > MAX_SAMPLES) {
        sample_list.pop_back();
        times_list.pop_back();
    }
}

//------------------------------------------------------------------------------
std::shared_ptr<OpenDynamicData> CommonData::copySample(const QString& topicName,
                                                        const unsigned int& index)
{
    std::shared_ptr<OpenDynamicData> newSample;
    QMutexLocker locker(&m_sampleMutex);

    if (m_samples.contains(topicName) &&
        m_samples.value(topicName).size() > (int)index)
    {
        // Don't copy the sample, just point to the shared pointer
        newSample = m_samples.value(topicName).at(index);
    }

    return newSample;
}

//------------------------------------------------------------------------------
DDS::DynamicData_var CommonData::copyDynamicSample(const QString& topic_name,
                                                   const unsigned int index)
{
    DDS::DynamicData_var copied;
    QMutexLocker locker(&m_dynamicSamplesMutex);

    if (m_dynamicSamples.contains(topic_name) &&
        m_dynamicSamples.value(topic_name).size() > (int)index) {
      copied = m_dynamicSamples.value(topic_name).at(index);
    }

    return copied;
}

//------------------------------------------------------------------------------
QStringList CommonData::getSampleList(const QString& topicName)
{
    QStringList sampleNames;
    QMutexLocker locker(&m_sampleMutex);

    if (m_sampleTimes.contains(topicName))
    {
        sampleNames = m_sampleTimes.value(topicName);
    }

    return sampleNames;
}


//------------------------------------------------------------------------------
void CommonData::clearSamples(const QString& topicName)
{
    QMutexLocker locker(&m_sampleMutex);
    if (m_samples.contains(topicName))
    {
        m_samples[topicName].clear();
        //QList<const OpenDynamicData*>& samples = m_samples[topicName];
        //while (!samples.empty())
        //{
        //    delete samples.back();
        //    samples.pop_back();
        //}

        while (!m_sampleTimes[topicName].empty())
        {
            m_sampleTimes[topicName].pop_back();
        }
    }
}


//------------------------------------------------------------------------------
TopicInfo::TopicInfo()
  : hasKey(true)
  , typeCodeLength(0)
  , typeCode(nullptr)
{
    topicQos = QosDictionary::Topic::bestEffort();
    writerQos = QosDictionary::DataWriter::bestEffort();
    readerQos = QosDictionary::DataReader::bestEffort();
    pubQos = QosDictionary::Publisher::defaultQos();
    subQos = QosDictionary::Subscriber::defaultQos();
    extensibility = OpenDDS::DCPS::Extensibility::APPENDABLE; //Set in TopicInfo::storeUserData
}


//------------------------------------------------------------------------------
TopicInfo::~TopicInfo()
{
    typeCode = nullptr;
}

//------------------------------------------------------------------------------
void TopicInfo::addPartitions(const DDS::PartitionQosPolicy& partitionQos)
{
    bool changeFound = false;

    // Add any new partitions to the partition list for this topic
    const DDS::StringSeq& partitionNames = partitionQos.name;
    for (CORBA::ULong i = 0; i < partitionNames.length(); i++)
    {
        QString partitionString(partitionNames[i].in());
        if (!partitions.contains(partitionString))
        {
            partitions.push_back(partitionString);
            changeFound = true;
        }
    }

    if (changeFound)
    {
        partitions.sort();
    }
}


//------------------------------------------------------------------------------
void TopicInfo::setDurabilityPolicy(const DDS::DurabilityQosPolicy& policy)
{
    topicQos.durability = policy;
    writerQos.durability = policy;
    readerQos.durability = policy;
}


//------------------------------------------------------------------------------
void TopicInfo::setDurabilityServicePolicy(const DDS::DurabilityServiceQosPolicy& policy)
{
    topicQos.durability_service = policy;
    writerQos.durability_service = policy;
}


//------------------------------------------------------------------------------
void TopicInfo::setDeadlinePolicy(const DDS::DeadlineQosPolicy& policy)
{
    topicQos.deadline = policy;
    writerQos.deadline = policy;
    readerQos.deadline = policy;
}


//------------------------------------------------------------------------------
void TopicInfo::setLatencyBudgePolicy(const DDS::LatencyBudgetQosPolicy& policy)
{
    topicQos.latency_budget = policy;
    writerQos.latency_budget = policy;
    readerQos.latency_budget = policy;
}


//------------------------------------------------------------------------------
void TopicInfo::setLifespanPolicy(const DDS::LifespanQosPolicy& policy)
{
    topicQos.lifespan = policy;
    writerQos.lifespan = policy;
}


//------------------------------------------------------------------------------
void TopicInfo::setLivelinessPolicy(const DDS::LivelinessQosPolicy& policy)
{
    topicQos.liveliness = policy;
    writerQos.liveliness = policy;
    readerQos.liveliness = policy;
}


//------------------------------------------------------------------------------
void TopicInfo::setReliabilityPolicy(const DDS::ReliabilityQosPolicy& policy)
{
    topicQos.reliability = policy;
    writerQos.reliability = policy;
    readerQos.reliability = policy;
}


//------------------------------------------------------------------------------
void TopicInfo::setOwnershipPolicy(const DDS::OwnershipQosPolicy& policy)
{
    topicQos.ownership = policy;
    writerQos.ownership = policy;
    readerQos.ownership = policy;
}


//------------------------------------------------------------------------------
void TopicInfo::setDestinationOrderPolicy(const DDS::DestinationOrderQosPolicy& policy)
{
    topicQos.destination_order = policy;
    writerQos.destination_order = policy;
    readerQos.destination_order = policy;
}


//------------------------------------------------------------------------------
void TopicInfo::setPresentationPolicy(const DDS::PresentationQosPolicy& policy)
{
    pubQos.presentation = policy;
    subQos.presentation = policy;
}

//------------------------------------------------------------------------------
void TopicInfo::fixHistory()
{
    if (topicQos.durability.kind == DDS::VOLATILE_DURABILITY_QOS &&
        topicQos.reliability.kind == DDS::RELIABLE_RELIABILITY_QOS)
    {
        //std::cout << "DEBUG STRICT RELIABLE TOPIC" << std::endl;
        topicQos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;
    }

    if (readerQos.durability.kind == DDS::VOLATILE_DURABILITY_QOS &&
        readerQos.reliability.kind == DDS::RELIABLE_RELIABILITY_QOS)
    {
        //std::cout << "DEBUG STRICT RELIABLE TOPIC" << std::endl;
        readerQos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;
    }

    if (writerQos.durability.kind == DDS::VOLATILE_DURABILITY_QOS &&
        writerQos.reliability.kind == DDS::RELIABLE_RELIABILITY_QOS)
    {
        //std::cout << "DEBUG STRICT RELIABLE WRITER" << std::endl;
        writerQos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;
    }
}


//------------------------------------------------------------------------------
void TopicInfo::storeUserData(const DDS::OctetSeq& userData)
{
    //std::cout << "DEBUG TopicInfo::storeUserData" << std::endl;
    // If no user data is found, we're done
    if (userData.length() == 0)
    {
        return;
    }

    // If we already know about this user data, we're done
    if (this->typeCode != nullptr)
    {
        return;
    }

    //std::cout << "DEBUG TopicInfo::storeUserData is new and non blank" << std::endl;
    // Begin parsing the user data byte array
    const size_t headerSize = 8;
    const size_t totalSize = userData.length();
    const size_t typeCodeSize = totalSize - headerSize;
    char header[headerSize];

    // If the user data is smaller than our header, it's definitely not ours
    if (totalSize <= headerSize)
    {
        return;
    }

    //std::cout << "DEBUG TopicInfo::storeUserData is not rejected due to size" << std::endl;

    memcpy(header, &userData[0], headerSize);

    // Check for the 'USR' header to make sure it's ours
    if (strncmp(header, "USR", 3) != 0)
    {
        return;
    }

    // If we got here, it's an USR user data type.
    // Format:
    // char[0-3] - USR user_data header
    // char[5]   - Topic has a key flag
    // char[6]   - Topic Type extensibility and mutability flags
    //           - 0: APPENDABLE
    //           - 1: FINAL
    //           - 2: MUTABLE
    // char[7-8] - Padding
    // char[8-N] - Serialized CDR topic typecode

    // Read USR data flags
    this->hasKey = (header[5] == 1);
    switch(header[6])
    {
        case 0:
            //std::cout << "APPENDABLE" << std::endl;
            this->extensibility = OpenDDS::DCPS::Extensibility::APPENDABLE;
            break;
        case 1:
            //std::cout << "FINAL" << std::endl;
            this->extensibility = OpenDDS::DCPS::Extensibility::FINAL;
            break;
        case 2:
            //std::cout << "MUTABLE" << std::endl;
            this->extensibility = OpenDDS::DCPS::Extensibility::MUTABLE; //Not supported at this time
            break;
        default:
            printf("Failed to demarshal extensibility %d from topic type %s\n", header[6], this->name.c_str());
            return;
    }
    // Create a typecode object from the CDR after the header
    const char* cdrBuffer = (char*)(&userData[headerSize]);
    TAO_InputCDR topicTypeIn(cdrBuffer, typeCodeSize);

    this->typeCodeObj = std::make_unique<CORBA::Any>();
    bool pass = (topicTypeIn >> *this->typeCodeObj);
    if (!pass)
    {
        printf("Failed to demarshal topic type %s from CDR\n", this->name.c_str());

        return;
    }

    // printf("Successfully demarshaled topic type from CDR\n");
    // printf("\n=== Begin CDR Dump ===\n");
    // for (size_t i = 0; i < (typeCodeSize+16); i+= 16)
    // {
    //     printf("%04X ", i);
    //     for(size_t j = 0; j < 16; j++)
    //     {
    //         if(j == 8)
    //             printf(" ");
    //         if((i+j) < typeCodeSize)
    //             printf("%02X", (uint8_t)cdrBuffer[i+j]);
    //         else
    //             printf("  ");
    //     }
    //     printf("   ");
    //     for(size_t j = 0; j < 16; j++)
    //     {
    //         if(j == 8)
    //             printf(" ");
    //         if(((i+j) < typeCodeSize) && isprint(cdrBuffer[i+j]))
    //             printf("%c", cdrBuffer[i+j]);
    //         else
    //             printf(" ");
    //     }
    //     printf("\n");
    // }
    // printf("\n=== End CDR Dump ===\n");
    this->typeCodeLength = typeCodeSize;
    this->typeCode = this->typeCodeObj->type();

} // End TopicInfo::storeUserData


//------------------------------------------------------------------------------
CommonData::CommonData()
{}


//------------------------------------------------------------------------------
CommonData::~CommonData()
{}


/**
 * @}
 */
