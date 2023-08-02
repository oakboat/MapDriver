#include <ntifs.h>
#include <cstdint>

#define DEV_NAME L"\\Device\\WmapDriver"
#define SYM_LINK_NAME L"\\??\\Wmap"

#define IOCTL_BASE 0x800
#define IOCTL_CODE(i) CTL_CODE(FILE_DEVICE_UNKNOWN,IOCTL_BASE+i,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define IOCTL_COPY_MEMORY IOCTL_CODE(1)
#define IOCTL_FILL_MEMORY IOCTL_CODE(2)
#define IOCTL_GET_PHYS_ADDRESS IOCTL_CODE(3)
#define IOCTL_MAP_IO_SPACE IOCTL_CODE(4)
#define IOCTL_UNMAP_IO_SPACE IOCTL_CODE(5)

typedef struct WRIO
{
	uint64_t source;
	uint64_t destination;
	uint64_t length;
	uint64_t address_to_translate;
	uint64_t return_physical_address;
	uint64_t physical_address_to_map;
	uint64_t return_virtual_address;
	uint64_t virt_address;
	uint32_t value;
	uint32_t size;
	uint32_t number_of_bytes;
}WRIO, * PWRIO;

NTSTATUS CreateMyDevice(IN PDRIVER_OBJECT pDriverObject) {
	NTSTATUS status;
	UNICODE_STRING devName;				//�豸����
	UNICODE_STRING sysLinkName;			//ϵͳ����������
	PDEVICE_OBJECT pDevObject;				//���ڷ��ش����豸

	RtlInitUnicodeString(&devName, DEV_NAME);
	status = IoCreateDevice(pDriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &pDevObject);
	if (!NT_SUCCESS(status)) {						//�жϴ����豸�Ƿ�ɹ�
		if (status == STATUS_INSUFFICIENT_RESOURCES)
			KdPrint(("��Դ����\n"));
		if (status == STATUS_OBJECT_NAME_EXISTS)
			KdPrint(("ָ������������\n"));
		if (status == STATUS_OBJECT_NAME_COLLISION)
			KdPrint(("�������г�ͻ\n"));
		return status;
	}
	KdPrint(("�豸�����ɹ�\n"));

	pDevObject->Flags |= DO_BUFFERED_IO;	//��������ʽ��д
	RtlInitUnicodeString(&sysLinkName, SYM_LINK_NAME);
	IoDeleteSymbolicLink(&sysLinkName);		//��ֹ������ͬ���������ظ�
	status = IoCreateSymbolicLink(&sysLinkName, &devName);		//�ж����ɷ��������Ƿ�ɹ�

	if (!NT_SUCCESS(status)) {
		KdPrint(("���ɷ�������ʧ��\n"));
		IoDeleteDevice(pDevObject);
		return status;
	}
	KdPrint(("���ɷ������ӳɹ�\n"));
	return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT pDriverObject) {
	PDEVICE_OBJECT pDevObject;
	UNICODE_STRING sysLinkName;

	pDevObject = pDriverObject->DeviceObject;
	IoDeleteDevice(pDevObject);	//ȡ���豸��ɾ��
	KdPrint(("�ɹ�ɾ���豸\n"));

	RtlInitUnicodeString(&sysLinkName, SYM_LINK_NAME);
	IoDeleteSymbolicLink(&sysLinkName);	//ȡ�÷������Ӳ�ɾ��
	KdPrint(("�ɹ�ɾ����������\n"));

	KdPrint(("�����ɹ�ж��\n"));
}

NTSTATUS OtherCompleteRoutine(PDEVICE_OBJECT pDriverObj, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS ControlDispatchRoutine(IN PDEVICE_OBJECT pDevobj, IN PIRP pIrp) {
	NTSTATUS Status;
	Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG_PTR InLength = IrpStack->Parameters.DeviceIoControl.InputBufferLength;   //��ȡ���뻺������С
	ULONG_PTR OutLength = IrpStack->Parameters.DeviceIoControl.OutputBufferLength; //��ȡ�����������С
	ULONG_PTR IoControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;  //�õ�IOCTL��
	PWRIO pIoBuffer = reinterpret_cast<PWRIO>(pIrp->AssociatedIrp.SystemBuffer);
	ULONG_PTR info = 0;
	NTSTATUS status = STATUS_SUCCESS;

	switch (IoControlCode)
	{
	case IOCTL_COPY_MEMORY:
	{
		KdPrint(("�����ڴ�"));
		RtlCopyMemory((void*)pIoBuffer->destination, (void*)pIoBuffer->source, pIoBuffer->length);
	}
	break;
	case IOCTL_FILL_MEMORY:
	{
		KdPrint(("����ڴ�"));
		RtlFillMemory((void*)pIoBuffer->destination, pIoBuffer->length, pIoBuffer->value);
	}
	break;
	case IOCTL_GET_PHYS_ADDRESS:
	{
		KdPrint(("��ȡ�����ַ�ڴ�"));
		pIoBuffer->return_physical_address = (uint64_t)MmGetPhysicalAddress((void*)pIoBuffer->address_to_translate).QuadPart;
		info = sizeof(WRIO);
	}
	break;
	case IOCTL_MAP_IO_SPACE:
	{
		KdPrint(("ӳ���ڴ�"));
		PHYSICAL_ADDRESS physicalAddress{ 0 };
		physicalAddress.QuadPart = pIoBuffer->physical_address_to_map;

		pIoBuffer->return_virtual_address = (uint64_t)MmMapIoSpace(physicalAddress, pIoBuffer->size, MmNonCached);
		info = sizeof(WRIO);
	}
	break;
	case IOCTL_UNMAP_IO_SPACE:
	{
		KdPrint(("ȡ��ӳ���ڴ�"));
		MmUnmapIoSpace((void*)pIoBuffer->virt_address, pIoBuffer->number_of_bytes);
	}
	break;
	default:
		KdPrint(("��������"));
		break;
	}

	pIrp->IoStatus.Information = info;			//���ò������ֽ���Ϊ0��������ʵ������
	pIrp->IoStatus.Status = STATUS_SUCCESS;		//���سɹ�
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);	//ָʾ��ɴ�IRP
	KdPrint(("�뿪��ǲ����\n"));				//������Ϣ
	return STATUS_SUCCESS;
}


extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING reg_path) {
	KdPrint(("�����ɹ�����\n"));
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = OtherCompleteRoutine;	//������ǲ���̣�Ҳ�ɷֿ��ò�ͬ����
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = OtherCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_READ] = OtherCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = OtherCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ControlDispatchRoutine;
	CreateMyDevice(pDriverObject);
	pDriverObject->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}